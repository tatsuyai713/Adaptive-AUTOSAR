/// @file src/ara/nm/cantp_nm_adapter.cpp
/// @brief CAN-TP NM transport adapter implementation.

#include "./cantp_nm_adapter.h"

#ifdef __linux__
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#elif defined(__QNX__)
#include <fcntl.h>
#include <devctl.h>
#include <unistd.h>
// QNX CAN resource-manager interface — /dev/can<N>/...
// struct can_msg equivalent is used through a portable shim below.
#endif

#include <algorithm>
#include <cstring>

namespace ara
{
    namespace nm
    {
        CanTpNmAdapter::CanTpNmAdapter(CanNmConfig config)
            : mConfig{std::move(config)}
        {
        }

        CanTpNmAdapter::~CanTpNmAdapter()
        {
            Stop();
        }

        void CanTpNmAdapter::MapChannel(
            const std::string &channelName, uint32_t canIdOffset)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            uint32_t canId = mConfig.BaseCanId + canIdOffset;
            mChannelToCanId[channelName] = canId;
            mCanIdToChannel[canId] = channelName;
        }

        core::Result<void> CanTpNmAdapter::SendNmPdu(
            const std::string &channelName,
            const std::vector<uint8_t> &pduData)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            auto it = mChannelToCanId.find(channelName);
            if (it == mChannelToCanId.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            if (pduData.size() > mConfig.MaxPayload)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

#ifdef __linux__
            if (mSocket < 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kNotStarted));
            }

            struct can_frame frame;
            std::memset(&frame, 0, sizeof(frame));
            frame.can_id = it->second;
            if (mConfig.UseExtendedId)
            {
                frame.can_id |= CAN_EFF_FLAG;
            }
            frame.can_dlc = static_cast<uint8_t>(
                std::min(pduData.size(), static_cast<size_t>(mConfig.MaxPayload)));
            std::memcpy(frame.data, pduData.data(), frame.can_dlc);

            ssize_t nbytes = ::write(mSocket, &frame, sizeof(frame));
            if (nbytes != sizeof(frame))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }
#elif defined(__QNX__)
            if (mTxFd < 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kNotStarted));
            }

            // QNX CAN: write a raw frame via the resource-manager tx path.
            // Frame format: [4B CAN-ID (network order)] [1B DLC] [0-8B data]
            const uint32_t canId = it->second;
            const uint8_t dlc = static_cast<uint8_t>(
                std::min(pduData.size(), static_cast<size_t>(mConfig.MaxPayload)));
            uint8_t buf[4 + 1 + 8];
            std::memset(buf, 0, sizeof(buf));
            buf[0] = static_cast<uint8_t>((canId >> 24) & 0xFF);
            buf[1] = static_cast<uint8_t>((canId >> 16) & 0xFF);
            buf[2] = static_cast<uint8_t>((canId >>  8) & 0xFF);
            buf[3] = static_cast<uint8_t>((canId      ) & 0xFF);
            if (mConfig.UseExtendedId)
            {
                buf[0] |= 0x80; // EFF flag in MSB
            }
            buf[4] = dlc;
            std::memcpy(buf + 5, pduData.data(), dlc);

            ssize_t nbytes = ::write(mTxFd, buf, 5 + dlc);
            if (nbytes < 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }
#endif

            ++mSentCount;
            return core::Result<void>::FromValue();
        }

        core::Result<void> CanTpNmAdapter::RegisterReceiveHandler(
            NmPduHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        core::Result<void> CanTpNmAdapter::Start()
        {
            if (mRunning.load())
            {
                return core::Result<void>::FromValue();
            }

#ifdef __linux__
            mSocket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
            if (mSocket < 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }

            struct ifreq ifr;
            std::memset(&ifr, 0, sizeof(ifr));
            std::strncpy(ifr.ifr_name, mConfig.InterfaceName.c_str(),
                         sizeof(ifr.ifr_name) - 1);

            if (::ioctl(mSocket, SIOCGIFINDEX, &ifr) < 0)
            {
                ::close(mSocket);
                mSocket = -1;
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }

            struct sockaddr_can addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.can_family = AF_CAN;
            addr.can_ifindex = ifr.ifr_ifindex;

            if (::bind(mSocket, reinterpret_cast<struct sockaddr *>(&addr),
                       sizeof(addr)) < 0)
            {
                ::close(mSocket);
                mSocket = -1;
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }
#elif defined(__QNX__)
            // QNX CAN: open the resource-manager rx path for the interface.
            // Convention: /dev/can<N>/rx0 for receive, /dev/can<N>/tx0 for transmit.
            std::string rxPath = "/dev/" + mConfig.InterfaceName + "/rx0";
            mSocket = ::open(rxPath.c_str(), O_RDONLY | O_NONBLOCK);
            if (mSocket < 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }

            std::string txPath = "/dev/" + mConfig.InterfaceName + "/tx0";
            mTxFd = ::open(txPath.c_str(), O_WRONLY);
            if (mTxFd < 0)
            {
                ::close(mSocket);
                mSocket = -1;
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kTransportError));
            }
#endif

            mRunning.store(true);
            mReceiveThread = std::thread(&CanTpNmAdapter::ReceiveLoop, this);
            return core::Result<void>::FromValue();
        }

        void CanTpNmAdapter::Stop()
        {
            mRunning.store(false);
            if (mReceiveThread.joinable())
            {
                mReceiveThread.join();
            }
#ifdef __linux__
            if (mSocket >= 0)
            {
                ::close(mSocket);
                mSocket = -1;
            }
#elif defined(__QNX__)
            if (mTxFd >= 0)
            {
                ::close(mTxFd);
                mTxFd = -1;
            }
            if (mSocket >= 0)
            {
                ::close(mSocket);
                mSocket = -1;
            }
#endif
        }

        bool CanTpNmAdapter::IsRunning() const noexcept
        {
            return mRunning.load();
        }

        uint64_t CanTpNmAdapter::GetSentCount() const noexcept
        {
            return mSentCount.load();
        }

        uint64_t CanTpNmAdapter::GetReceivedCount() const noexcept
        {
            return mReceivedCount.load();
        }

        void CanTpNmAdapter::ReceiveLoop()
        {
#ifdef __linux__
            struct pollfd pfd;
            pfd.fd = mSocket;
            pfd.events = POLLIN;

            while (mRunning.load())
            {
                int ret = ::poll(&pfd, 1,
                                 static_cast<int>(mConfig.PollTimeoutMs));
                if (ret <= 0)
                    continue;

                struct can_frame frame;
                ssize_t nbytes = ::read(mSocket, &frame, sizeof(frame));
                if (nbytes != sizeof(frame))
                    continue;

                uint32_t rawId = frame.can_id & CAN_EFF_MASK;
                std::string channel = FindChannelByCanId(rawId);
                if (channel.empty())
                    continue;

                std::vector<uint8_t> data(frame.data,
                                          frame.data + frame.can_dlc);
                ++mReceivedCount;

                NmPduHandler handler;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    handler = mHandler;
                }
                if (handler)
                {
                    handler(channel, data);
                }
            }
#elif defined(__QNX__)
            // QNX CAN resource-manager receive loop.
            // Read raw frames from /dev/can<N>/rx0.
            // Frame wire format: [4B CAN-ID (network order)] [1B DLC] [0-8B data]
            while (mRunning.load())
            {
                uint8_t buf[4 + 1 + 8];
                ssize_t n = ::read(mSocket, buf, sizeof(buf));
                if (n < 5)
                {
                    // No frame available or short read — wait and retry
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(mConfig.PollTimeoutMs));
                    continue;
                }

                uint32_t rawId = (static_cast<uint32_t>(buf[0] & 0x7F) << 24)
                               | (static_cast<uint32_t>(buf[1]) << 16)
                               | (static_cast<uint32_t>(buf[2]) <<  8)
                               |  static_cast<uint32_t>(buf[3]);
                uint8_t dlc = buf[4];
                if (dlc > 8) dlc = 8;

                std::string channel = FindChannelByCanId(rawId);
                if (channel.empty())
                    continue;

                std::vector<uint8_t> data(buf + 5, buf + 5 + dlc);
                ++mReceivedCount;

                NmPduHandler handler;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    handler = mHandler;
                }
                if (handler)
                {
                    handler(channel, data);
                }
            }
#else
            while (mRunning.load())
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(mConfig.PollTimeoutMs));
            }
#endif
        }

        std::string CanTpNmAdapter::FindChannelByCanId(uint32_t canId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCanIdToChannel.find(canId);
            if (it != mCanIdToChannel.end())
            {
                return it->second;
            }
            return {};
        }
    }
}
