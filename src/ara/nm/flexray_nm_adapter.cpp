/// @file src/ara/nm/flexray_nm_adapter.cpp
/// @brief FlexRay NM transport adapter implementation.

#include "./flexray_nm_adapter.h"
#include <algorithm>
#include <chrono>

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
                // Simulate FlexRay communication cycle processing
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mPendingTx.clear();
                }
                std::this_thread::sleep_for(cyclePeriod);
            }
        }
    }
}
