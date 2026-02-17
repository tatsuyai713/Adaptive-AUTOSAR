/// @file src/ara/tsync/time_sync_server.cpp
/// @brief Implementation for ara::tsync::TimeSyncServer.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./time_sync_server.h"
#include <algorithm>

namespace ara
{
    namespace tsync
    {
        // -----------------------------------------------------------------------
        // Constructor / Destructor
        // -----------------------------------------------------------------------
        TimeSyncServer::TimeSyncServer(
            SynchronizedTimeBaseProvider &provider,
            const TimeSyncServerConfig &config) noexcept
            : mProvider{provider}, mConfig{config}
        {
        }

        TimeSyncServer::~TimeSyncServer()
        {
            Stop();
        }

        // -----------------------------------------------------------------------
        // Start / Stop
        // -----------------------------------------------------------------------
        core::Result<void> TimeSyncServer::Start()
        {
            if (mRunning.load())
            {
                return core::Result<void>::FromError(MakeErrorCode(TsyncErrc::kInvalidArgument));
            }
            mRunning.store(true);
            mConsecutiveFailures = 0;
            mPollThread = std::thread(&TimeSyncServer::pollLoop, this);
            return {};
        }

        void TimeSyncServer::Stop()
        {
            if (!mRunning.load())
            {
                return;
            }
            mRunning.store(false);
            if (mPollThread.joinable())
            {
                mPollThread.join();
            }
        }

        // -----------------------------------------------------------------------
        // Consumer management
        // -----------------------------------------------------------------------
        core::Result<void> TimeSyncServer::RegisterConsumer(TimeSyncClient &consumer)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mConsumers.push_back(&consumer);
            return {};
        }

        void TimeSyncServer::UnregisterConsumer(TimeSyncClient &consumer)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mConsumers.erase(
                std::remove(mConsumers.begin(), mConsumers.end(), &consumer),
                mConsumers.end());
        }

        // -----------------------------------------------------------------------
        // Availability callback
        // -----------------------------------------------------------------------
        void TimeSyncServer::SetAvailabilityCallback(AvailabilityCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mAvailabilityCallback = std::move(cb);
        }

        // -----------------------------------------------------------------------
        // Accessors
        // -----------------------------------------------------------------------
        core::Result<std::chrono::system_clock::time_point>
        TimeSyncServer::GetCurrentTime() const
        {
            return mInternalClient.GetCurrentTime();
        }

        bool TimeSyncServer::IsProviderAvailable() const noexcept
        {
            return mProviderAvailable.load();
        }

        uint32_t TimeSyncServer::GetPollIntervalMs() const noexcept
        {
            return mConfig.pollIntervalMs;
        }

        void TimeSyncServer::SetPollIntervalMs(uint32_t ms) noexcept
        {
            mConfig.pollIntervalMs = (ms >= 10U) ? ms : 10U;
        }

        // -----------------------------------------------------------------------
        // pollLoop — background polling
        // -----------------------------------------------------------------------
        void TimeSyncServer::pollLoop()
        {
            while (mRunning.load())
            {
                const auto sleepDuration =
                    std::chrono::milliseconds(mConfig.pollIntervalMs);
                std::this_thread::sleep_for(sleepDuration);

                if (!mRunning.load()) break;

                // Capture steady time before and after provider query
                const auto steadyBefore = std::chrono::steady_clock::now();
                auto updateResult = mProvider.UpdateTimeBase(mInternalClient);
                const auto steadyAfter = std::chrono::steady_clock::now();
                // Use midpoint of before/after as the steady-clock reference
                const auto steadyRef = steadyBefore +
                    (steadyAfter - steadyBefore) / 2;

                if (updateResult.HasValue())
                {
                    mConsecutiveFailures = 0;

                    if (!mProviderAvailable.load())
                    {
                        mProviderAvailable.store(true);
                        notifyAvailability(true);
                    }

                    // Propagate to all registered consumers
                    auto globalResult = mInternalClient.GetCurrentTime(steadyRef);
                    if (globalResult.HasValue())
                    {
                        distributeToConsumers(globalResult.Value(), steadyRef);
                    }
                }
                else
                {
                    ++mConsecutiveFailures;
                    if (mConsecutiveFailures >= mConfig.maxFailureCount &&
                        mProviderAvailable.load())
                    {
                        mProviderAvailable.store(false);
                        notifyAvailability(false);
                        handleProviderLoss();
                    }
                }
            }
        }

        // -----------------------------------------------------------------------
        // distributeToConsumers
        // -----------------------------------------------------------------------
        void TimeSyncServer::distributeToConsumers(
            std::chrono::system_clock::time_point globalTime,
            std::chrono::steady_clock::time_point steadyTime)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (TimeSyncClient *consumer : mConsumers)
            {
                if (consumer != nullptr)
                {
                    consumer->UpdateReferenceTime(globalTime, steadyTime);
                }
            }
        }

        // -----------------------------------------------------------------------
        // handleProviderLoss — reset all consumers on provider loss
        // -----------------------------------------------------------------------
        void TimeSyncServer::handleProviderLoss()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (TimeSyncClient *consumer : mConsumers)
            {
                if (consumer != nullptr)
                {
                    consumer->Reset();
                }
            }
            mInternalClient.Reset();
        }

        // -----------------------------------------------------------------------
        // notifyAvailability
        // -----------------------------------------------------------------------
        void TimeSyncServer::notifyAvailability(bool available)
        {
            AvailabilityCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                cb = mAvailabilityCallback;
            }
            if (cb)
            {
                cb(available);
            }
        }

    } // namespace tsync
} // namespace ara
