/// @file src/ara/phm/alive_supervision.cpp
/// @brief Alive Supervision implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./alive_supervision.h"

namespace ara
{
    namespace phm
    {
        // -----------------------------------------------------------------------
        // Constructor / Destructor
        // -----------------------------------------------------------------------
        AliveSupervision::AliveSupervision(const AliveSupervisionConfig &config)
            : mConfig{config},
              mStatus{AliveSupervisionStatus::kDeactivated},
              mLastCheckpointTime{std::chrono::steady_clock::now()}
        {
        }

        AliveSupervision::~AliveSupervision()
        {
            Stop();
        }

        // -----------------------------------------------------------------------
        // Start / Stop
        // -----------------------------------------------------------------------
        void AliveSupervision::Start()
        {
            if (mRunning.load())
            {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mLastCheckpointTime = std::chrono::steady_clock::now();
                mCheckpointReported.store(false);
                mFailedCount = 0;
                mPassedCount = 0;
            }
            mRunning.store(true);
            updateStatus(AliveSupervisionStatus::kOk);
            mMonitorThread = std::thread(&AliveSupervision::monitorLoop, this);
        }

        void AliveSupervision::Stop()
        {
            if (!mRunning.load())
            {
                return;
            }
            mRunning.store(false);
            if (mMonitorThread.joinable())
            {
                mMonitorThread.join();
            }
            updateStatus(AliveSupervisionStatus::kDeactivated);
        }

        // -----------------------------------------------------------------------
        // ReportCheckpoint — called by the supervised entity
        // -----------------------------------------------------------------------
        void AliveSupervision::ReportCheckpoint()
        {
            if (!mRunning.load())
            {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mLastCheckpointTime = now;
                mCheckpointReported.store(true);
            }
        }

        // -----------------------------------------------------------------------
        // GetStatus
        // -----------------------------------------------------------------------
        AliveSupervisionStatus AliveSupervision::GetStatus() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mStatus;
        }

        // -----------------------------------------------------------------------
        // SetStatusCallback
        // -----------------------------------------------------------------------
        void AliveSupervision::SetStatusCallback(StatusCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStatusCallback = std::move(callback);
        }

        // -----------------------------------------------------------------------
        // updateStatus — calls callback if status changed
        // -----------------------------------------------------------------------
        void AliveSupervision::updateStatus(AliveSupervisionStatus newStatus)
        {
            StatusCallback cb;
            bool changed = false;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mStatus != newStatus)
                {
                    mStatus = newStatus;
                    cb = mStatusCallback;
                    changed = true;
                }
            }
            if (changed && cb)
            {
                cb(newStatus);
            }
        }

        // -----------------------------------------------------------------------
        // monitorLoop — background window checker
        // -----------------------------------------------------------------------
        void AliveSupervision::monitorLoop()
        {
            const auto windowDuration = std::chrono::milliseconds(mConfig.alivePeriodMs);
            const auto minInterval = std::chrono::milliseconds(
                static_cast<long long>(mConfig.alivePeriodMs * mConfig.minMargin));
            const auto maxInterval = std::chrono::milliseconds(
                static_cast<long long>(mConfig.alivePeriodMs * mConfig.maxMargin));

            auto windowStart = std::chrono::steady_clock::now();

            while (mRunning.load())
            {
                // Sleep for one supervision window
                std::this_thread::sleep_for(windowDuration);

                const auto windowEnd = std::chrono::steady_clock::now();

                bool checkpointReceived;
                std::chrono::steady_clock::time_point lastCheckpoint;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    checkpointReceived = mCheckpointReported.load();
                    lastCheckpoint = mLastCheckpointTime;
                    mCheckpointReported.store(false); // reset for next window
                }

                bool windowPassed = false;

                if (!checkpointReceived)
                {
                    // No checkpoint received in this window → FAILED
                    windowPassed = false;
                }
                else
                {
                    // Check if interval was within [min, max]
                    const auto interval =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            lastCheckpoint - windowStart);
                    windowPassed = (interval >= minInterval && interval <= maxInterval);
                }

                windowStart = windowEnd;

                if (windowPassed)
                {
                    uint32_t passed;
                    uint32_t failed;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        ++mPassedCount;
                        mFailedCount = 0;
                        passed = mPassedCount;
                        failed = mFailedCount;
                    }
                    (void)failed;
                    if (passed >= mConfig.passedThreshold)
                    {
                        updateStatus(AliveSupervisionStatus::kOk);
                    }
                }
                else
                {
                    uint32_t failed;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        ++mFailedCount;
                        mPassedCount = 0;
                        failed = mFailedCount;
                    }

                    if (failed >= mConfig.failedThreshold)
                    {
                        updateStatus(AliveSupervisionStatus::kExpired);
                    }
                    else
                    {
                        updateStatus(AliveSupervisionStatus::kFailed);
                    }
                }
            }
        }

    } // namespace phm
} // namespace ara
