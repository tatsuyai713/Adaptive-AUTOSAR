#include "./socketcan_receiver.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "ara/com/com_error_domain.h"

#if defined(__linux__)
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
    // Helper to convert local failures into ara::com error domain values.
    ara::core::ErrorCode MakeComError(ara::com::ComErrc code) noexcept
    {
        return ara::com::MakeErrorCode(code);
    }
}

namespace sample
{
    namespace ara_com_socketcan_gateway
    {
        SocketCanReceiver::SocketCanReceiver(std::string interfaceName) noexcept
            : mInterfaceName{std::move(interfaceName)},
              mSocketFd{-1}
        {
        }

        SocketCanReceiver::SocketCanReceiver(SocketCanReceiver &&other) noexcept
            : mInterfaceName{std::move(other.mInterfaceName)},
              mSocketFd{other.mSocketFd}
        {
            other.mSocketFd = -1;
        }

        SocketCanReceiver &SocketCanReceiver::operator=(SocketCanReceiver &&other) noexcept
        {
            if (this != &other)
            {
                Close();
                mInterfaceName = std::move(other.mInterfaceName);
                mSocketFd = other.mSocketFd;
                other.mSocketFd = -1;
            }

            return *this;
        }

        SocketCanReceiver::~SocketCanReceiver() noexcept
        {
            Close();
        }

        ara::core::Result<void> SocketCanReceiver::Open()
        {
#if !defined(__linux__)
            // SocketCAN is Linux-specific in this sample implementation.
            return ara::core::Result<void>::FromError(
                MakeComError(ara::com::ComErrc::kCommunicationStackError));
#else
            if (mSocketFd >= 0)
            {
                return ara::core::Result<void>::FromValue();
            }

            if (mInterfaceName.empty() || mInterfaceName.size() >= IFNAMSIZ)
            {
                return ara::core::Result<void>::FromError(
                    MakeComError(ara::com::ComErrc::kFieldValueIsNotValid));
            }

            int socketFd{::socket(PF_CAN, SOCK_RAW, CAN_RAW)};
            if (socketFd < 0)
            {
                // Raw CAN socket creation failed.
                return ara::core::Result<void>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationStackError));
            }

            struct ifreq interfaceRequest;
            std::memset(&interfaceRequest, 0, sizeof(interfaceRequest));
            std::strncpy(
                interfaceRequest.ifr_name,
                mInterfaceName.c_str(),
                IFNAMSIZ - 1);

            if (::ioctl(socketFd, SIOCGIFINDEX, &interfaceRequest) < 0)
            {
                // Interface does not exist or cannot be resolved to index.
                ::close(socketFd);
                return ara::core::Result<void>::FromError(
                    MakeComError(ara::com::ComErrc::kFieldValueIsNotValid));
            }

            struct sockaddr_can address;
            std::memset(&address, 0, sizeof(address));
            address.can_family = AF_CAN;
            address.can_ifindex = interfaceRequest.ifr_ifindex;

            if (::bind(
                    socketFd,
                    reinterpret_cast<struct sockaddr *>(&address),
                    sizeof(address)) < 0)
            {
                // Interface exists but socket bind failed.
                ::close(socketFd);
                return ara::core::Result<void>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationLinkError));
            }

            mSocketFd = socketFd;
            return ara::core::Result<void>::FromValue();
#endif
        }

        void SocketCanReceiver::Close() noexcept
        {
#if defined(__linux__)
            if (mSocketFd >= 0)
            {
                ::close(mSocketFd);
                mSocketFd = -1;
            }
#else
            mSocketFd = -1;
#endif
        }

        ara::core::Result<bool> SocketCanReceiver::Receive(
            CanFrame &frame,
            std::chrono::milliseconds timeout)
        {
#if !defined(__linux__)
            // SocketCAN is Linux-specific in this sample implementation.
            (void)frame;
            (void)timeout;
            return ara::core::Result<bool>::FromError(
                MakeComError(ara::com::ComErrc::kCommunicationStackError));
#else
            if (mSocketFd < 0)
            {
                return ara::core::Result<bool>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationStackError));
            }

            struct pollfd pollDescriptor;
            std::memset(&pollDescriptor, 0, sizeof(pollDescriptor));
            pollDescriptor.fd = mSocketFd;
            pollDescriptor.events = POLLIN;

            const auto cTimeoutMsLong{timeout.count()};
            const int cTimeoutMs = cTimeoutMsLong < 0
                                       ? -1
                                       : static_cast<int>(
                                             std::min<std::int64_t>(
                                                 cTimeoutMsLong,
                                                 2147483647LL));

            int pollResult{::poll(&pollDescriptor, 1, cTimeoutMs)};
            if (pollResult == 0)
            {
                // Timeout without incoming CAN frame.
                return ara::core::Result<bool>::FromValue(false);
            }

            if (pollResult < 0)
            {
                if (errno == EINTR)
                {
                    // Interrupted by signal; caller can retry.
                    return ara::core::Result<bool>::FromValue(false);
                }

                return ara::core::Result<bool>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationLinkError));
            }

            if ((pollDescriptor.revents & POLLIN) == 0)
            {
                // Unexpected poll wake-up without readable data.
                return ara::core::Result<bool>::FromValue(false);
            }

            struct can_frame rawFrame;
            std::memset(&rawFrame, 0, sizeof(rawFrame));

            const ssize_t cReadBytes{
                ::read(mSocketFd, &rawFrame, sizeof(rawFrame))};

            if (cReadBytes < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Non-blocking-style transient condition.
                    return ara::core::Result<bool>::FromValue(false);
                }

                return ara::core::Result<bool>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationLinkError));
            }

            if (cReadBytes != static_cast<ssize_t>(sizeof(rawFrame)))
            {
                return ara::core::Result<bool>::FromError(
                    MakeComError(ara::com::ComErrc::kCommunicationStackError));
            }

            frame.IsExtended = (rawFrame.can_id & CAN_EFF_FLAG) != 0U;
            frame.IsRemote = (rawFrame.can_id & CAN_RTR_FLAG) != 0U;
            // Mask identifier according to frame format.
            frame.Id = frame.IsExtended
                           ? (rawFrame.can_id & CAN_EFF_MASK)
                           : (rawFrame.can_id & CAN_SFF_MASK);
            frame.Dlc = static_cast<std::uint8_t>(rawFrame.can_dlc);
            std::copy(
                std::begin(rawFrame.data),
                std::end(rawFrame.data),
                frame.Data.begin());
            frame.Timestamp = std::chrono::steady_clock::now();

            return ara::core::Result<bool>::FromValue(true);
#endif
        }

        const char *SocketCanReceiver::BackendName() const noexcept
        {
            return "socketcan";
        }
    }
}
