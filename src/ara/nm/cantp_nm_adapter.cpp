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
