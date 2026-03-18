/// @file src/ara/nm/flexray_nm_adapter.cpp
/// @brief FlexRay NM transport adapter implementation.

#include "./flexray_nm_adapter.h"
#include <algorithm>
#include <chrono>
#if defined(__linux__) || defined(__QNX__)
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <poll.h>
#include <sstream>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace ara
{
    namespace nm
    {
        FlexRayNmAdapter::FlexRayNmAdapter(FlexRayNmConfig config)
            : mConfig{std::move(config)}
        {
        }

        FlexRayNmAdapter::~FlexRayNmAdapter()
        {
            Stop();
        }

        void FlexRayNmAdapter::MapChannel(
            const std::string &channelName, FlexRayNmSlot slots)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mChannelSlots[channelName] = slots;
        }

        core::Result<void> FlexRayNmAdapter::SendNmPdu(
            const std::string &channelName,
            const std::vector<uint8_t> &pduData)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mChannelSlots.find(channelName) == mChannelSlots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            if (pduData.size() > mConfig.Timing.StaticPayloadBytes)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

            mPendingTx[channelName] = pduData;
            ++mSentCount;
            return core::Result<void>::FromValue();
        }

        core::Result<void> FlexRayNmAdapter::RegisterReceiveHandler(
            NmPduHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        core::Result<void> FlexRayNmAdapter::Start()
        {
            if (mRunning.load())
            {
                return core::Result<void>::FromValue();
            }

#if defined(__linux__) || defined(__QNX__)
            // Create a SOCK_DGRAM Unix socket to serve as the receive endpoint for
            // this node on the simulated FlexRay bus.  Multiple adapter instances
            // (same or different processes) sharing the same ClusterName can
            // exchange NM PDUs via their registered socket paths.
            mSocket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
            if (mSocket >= 0)
            {
                std::ostringstream oss;
                oss << "/tmp/flexray_" << mConfig.ClusterName
                    << "_" << ::getpid() << ".sock";
                mSocketPath = oss.str();

                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                ::strncpy(addr.sun_path, mSocketPath.c_str(),
                          sizeof(addr.sun_path) - 1);
                ::unlink(mSocketPath.c_str());

                if (::bind(mSocket,
                           reinterpret_cast<struct sockaddr *>(&addr),
                           sizeof(addr)) < 0)
                {
                    ::close(mSocket);
                    mSocket = -1;
                    mSocketPath.clear();
                }
                else
                {
                    // Register this node in the cluster peer list.
                    const std::string peerFile =
                        "/tmp/flexray_" + mConfig.ClusterName + ".peers";
                    int pfd = ::open(peerFile.c_str(),
                                     O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (pfd >= 0)
                    {
                        ::flock(pfd, LOCK_EX);
                        const std::string line = mSocketPath + "\n";
                        ::write(pfd, line.data(), line.size());
                        ::flock(pfd, LOCK_UN);
                        ::close(pfd);
                    }
                }
            }
#endif

            mRunning.store(true);
            mCycleThread = std::thread(&FlexRayNmAdapter::CycleLoop, this);
            return core::Result<void>::FromValue();
        }

        void FlexRayNmAdapter::Stop()
        {
            mRunning.store(false);
            if (mCycleThread.joinable())
            {
                mCycleThread.join();
            }

#if defined(__linux__) || defined(__QNX__)
            if (mSocket >= 0)
            {
                ::close(mSocket);
                mSocket = -1;

                if (!mSocketPath.empty())
                {
                    ::unlink(mSocketPath.c_str());

                    // Remove this node from the cluster peer list.
                    const std::string peerFile =
                        "/tmp/flexray_" + mConfig.ClusterName + ".peers";
                    std::ifstream ifs(peerFile);
                    std::vector<std::string> remaining;
                    std::string line;
                    while (std::getline(ifs, line))
                        if (!line.empty() && line != mSocketPath)
                            remaining.push_back(line);
                    ifs.close();

                    std::ofstream ofs(peerFile, std::ios::trunc);
                    for (const auto &p : remaining)
                        ofs << p << "\n";

                    mSocketPath.clear();
                }
            }
#endif
        }

        bool FlexRayNmAdapter::IsRunning() const noexcept
        {
            return mRunning.load();
        }

        const FlexRayTimingConfig &FlexRayNmAdapter::GetTiming() const noexcept
        {
            return mConfig.Timing;
        }

        uint64_t FlexRayNmAdapter::GetSentCount() const noexcept
        {
            return mSentCount.load();
        }

        uint64_t FlexRayNmAdapter::GetReceivedCount() const noexcept
        {
            return mReceivedCount.load();
        }

        void FlexRayNmAdapter::InjectReceivedPdu(
            const std::string &channelName,
            const std::vector<uint8_t> &pduData)
        {
            ++mReceivedCount;
            NmPduHandler handler;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                handler = mHandler;
            }
            if (handler)
            {
                handler(channelName, pduData);
            }
        }

        void FlexRayNmAdapter::CycleLoop()
        {
            const auto cyclePeriod = std::chrono::microseconds(
                mConfig.Timing.CyclePeriodUs);

            while (mRunning.load())
            {
#if defined(__linux__) || defined(__QNX__)
                // --- Phase 1: Transmit pending frames to all registered peers ---
                std::unordered_map<std::string, std::vector<uint8_t>> pending;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    pending.swap(mPendingTx);
                }

                if (mSocket >= 0 && !pending.empty())
                {
                    const std::string peerFile =
                        "/tmp/flexray_" + mConfig.ClusterName + ".peers";
                    std::ifstream peerStream(peerFile);
                    std::string peerPath;
                    while (std::getline(peerStream, peerPath))
                    {
                        if (peerPath.empty() || peerPath == mSocketPath)
                            continue;

                        struct sockaddr_un peerAddr{};
                        peerAddr.sun_family = AF_UNIX;
                        ::strncpy(peerAddr.sun_path, peerPath.c_str(),
                                  sizeof(peerAddr.sun_path) - 1);

                        for (const auto &kv : pending)
                        {
                            // Frame layout:
                            //   [1B channel name length]
                            //   [N bytes channel name]
                            //   [1B data length hi] [1B data length lo]
                            //   [M bytes PDU data]
                            const auto &ch   = kv.first;
                            const auto &data = kv.second;
                            std::vector<uint8_t> frame;
                            frame.push_back(
                                static_cast<uint8_t>(ch.size() & 0xFFu));
                            frame.insert(frame.end(), ch.begin(), ch.end());
                            frame.push_back(
                                static_cast<uint8_t>(data.size() >> 8u));
                            frame.push_back(
                                static_cast<uint8_t>(data.size() & 0xFFu));
                            frame.insert(frame.end(), data.begin(), data.end());
                            ::sendto(mSocket, frame.data(), frame.size(), 0,
                                     reinterpret_cast<const struct sockaddr *>(
                                         &peerAddr),
                                     sizeof(peerAddr));
                        }
                    }
                }

                // --- Phase 2: Receive incoming frames from remote peers ---
                if (mSocket >= 0)
                {
                    // Poll for the remaining fraction of the cycle period.
                    const int pollMs =
                        static_cast<int>(mConfig.Timing.CyclePeriodUs / 1000u);

                    struct pollfd pfd{};
                    pfd.fd     = mSocket;
                    pfd.events = POLLIN;

                    if (::poll(&pfd, 1, pollMs > 0 ? pollMs : 1) > 0 &&
                        (pfd.revents & POLLIN))
                    {
                        // Maximum frame: 1 + 255 + 2 + 254 = 512 bytes
                        uint8_t buf[512];
                        const ssize_t n = ::recvfrom(
                            mSocket, buf, sizeof(buf), 0, nullptr, nullptr);
                        if (n >= 3)
                        {
                            const uint8_t chLen = buf[0];
                            if (n >= 1 + chLen + 2)
                            {
                                std::string channelName(
                                    reinterpret_cast<char *>(&buf[1]), chLen);
                                const uint16_t dataLen =
                                    (static_cast<uint16_t>(buf[1 + chLen]) << 8u)
                                    | buf[2 + chLen];
                                if (n >= static_cast<ssize_t>(
                                        3u + chLen + dataLen))
                                {
                                    std::vector<uint8_t> data(
                                        &buf[3 + chLen],
                                        &buf[3 + chLen + dataLen]);
                                    ++mReceivedCount;
                                    NmPduHandler handler;
                                    {
                                        std::lock_guard<std::mutex> lock(
                                            mMutex);
                                        handler = mHandler;
                                    }
                                    if (handler)
                                        handler(channelName, data);
                                }
                            }
                        }
                    }
                }
                else
                {
                    std::this_thread::sleep_for(cyclePeriod);
                }
#else
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPendingTx.clear();
                }
                std::this_thread::sleep_for(cyclePeriod);
#endif
            }
        }
    }
}
