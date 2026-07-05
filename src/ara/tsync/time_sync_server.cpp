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
            : mProvider{provider},
              mConfig{config},
              mPollIntervalMs{config.pollIntervalMs >= 10U
                                  ? config.pollIntervalMs
                                  : 10U}
        {
            mConfig.pollIntervalMs = mPollIntervalMs.load();
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
            return mPollIntervalMs.load();
        }

        void TimeSyncServer::SetPollIntervalMs(uint32_t ms) noexcept
        {
            const uint32_t interval = (ms >= 10U) ? ms : 10U;
            mPollIntervalMs.store(interval);
        }

        // -----------------------------------------------------------------------
        // pollLoop — background polling
        // -----------------------------------------------------------------------
        void TimeSyncServer::pollLoop()
        {
            while (mRunning.load())
            {
                const auto sleepDuration =
                    std::chrono::milliseconds(mPollIntervalMs.load());
                std::this_thread::sleep_for(sleepDuration);

                if (!mRunning.load()) break;

                // Capture steady time before and after provider query
                const auto steadyBefore = std::chrono::steady_clock::now();
                const auto previousTime = mInternalClient.GetCurrentTime(steadyBefore);
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
                        if (previousTime.HasValue())
                        {
                            const auto steadyDelta =
                                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                    steadyRef - steadyBefore);
                            const auto expectedTime =
                                previousTime.Value() + steadyDelta;
                            const auto correction =
                                std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    globalResult.Value() - expectedTime);
                            if (correction.count() != 0)
                            {
                                notifyCorrection(correction, globalResult.Value());
                            }
                        }
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

        void TimeSyncServer::SetCorrectionCallback(CorrectionCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCorrectionCallback = std::move(cb);
        }

        void TimeSyncServer::notifyCorrection(
            std::chrono::nanoseconds correctionAmount,
            std::chrono::system_clock::time_point correctedTime)
        {
            CorrectionCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                cb = mCorrectionCallback;
            }
            if (cb)
            {
                cb(correctionAmount, correctedTime);
            }
        }

    } // namespace tsync
} // namespace ara
