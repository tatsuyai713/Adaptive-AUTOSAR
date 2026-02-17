/// @file src/ara/phm/deadline_supervision.cpp
/// @brief Deadline Supervision implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./deadline_supervision.h"

namespace ara
{
    namespace phm
    {
        DeadlineSupervision::DeadlineSupervision(const DeadlineSupervisionConfig &config)
            : mConfig{config},
              mStatus{DeadlineSupervisionStatus::kDeactivated},
              mWindowOpen{false},
              mStartTime{std::chrono::steady_clock::now()}
        {
        }

        DeadlineSupervision::~DeadlineSupervision()
        {
            Stop();
        }

        void DeadlineSupervision::Start()
        {
            if (mRunning.load()) return;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mWindowOpen = false;
                mFailedCount = 0;
                mPassedCount = 0;
            }
            mRunning.store(true);
            updateStatus(DeadlineSupervisionStatus::kOk);
            mWatcherThread = std::thread(&DeadlineSupervision::watcherLoop, this);
        }

        void DeadlineSupervision::Stop()
        {
            if (!mRunning.load()) return;
            mRunning.store(false);
            if (mWatcherThread.joinable()) mWatcherThread.join();
            updateStatus(DeadlineSupervisionStatus::kDeactivated);
        }

        void DeadlineSupervision::ReportStart()
        {
            if (!mRunning.load()) return;
            std::lock_guard<std::mutex> lock(mMutex);
            mStartTime = std::chrono::steady_clock::now();
            mWindowOpen = true;
        }

        void DeadlineSupervision::ReportEnd()
        {
            if (!mRunning.load()) return;

            const auto now = std::chrono::steady_clock::now();
            bool passed = false;
            bool hadOpenWindow = false;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                hadOpenWindow = mWindowOpen;
                if (mWindowOpen)
                {
                    const auto elapsedMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - mStartTime).count();
                    passed = (elapsedMs >= static_cast<long long>(mConfig.minDeadlineMs) &&
                              elapsedMs <= static_cast<long long>(mConfig.maxDeadlineMs));
                    mWindowOpen = false;
                }
            }

            if (hadOpenWindow)
            {
                recordResult(passed);
            }
            // If no window was open, ReportEnd without ReportStart → ignore.
        }

        DeadlineSupervisionStatus DeadlineSupervision::GetStatus() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mStatus;
        }

        void DeadlineSupervision::SetStatusCallback(StatusCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStatusCallback = std::move(callback);
        }

        // -----------------------------------------------------------------------
        // watcherLoop — background thread that detects expired deadlines
        // -----------------------------------------------------------------------
        void DeadlineSupervision::watcherLoop()
        {
            const auto checkInterval = std::chrono::milliseconds(10);

            while (mRunning.load())
            {
                std::this_thread::sleep_for(checkInterval);

                const auto now = std::chrono::steady_clock::now();
                bool expired = false;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mWindowOpen)
                    {
                        const auto elapsedMs =
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - mStartTime).count();
                        if (elapsedMs > static_cast<long long>(mConfig.maxDeadlineMs))
                        {
                            // Max deadline exceeded without ReportEnd → FAILED
                            mWindowOpen = false;
                            expired = true;
                        }
                    }
                }
                if (expired)
                {
                    recordResult(false);
                }
            }
        }

        // -----------------------------------------------------------------------
        // recordResult — update counters and status
        // -----------------------------------------------------------------------
        void DeadlineSupervision::recordResult(bool passed)
        {
            if (passed)
            {
                uint32_t passedCount;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    ++mPassedCount;
                    mFailedCount = 0;
                    passedCount = mPassedCount;
                }
                if (passedCount >= mConfig.passedThreshold)
                {
                    updateStatus(DeadlineSupervisionStatus::kOk);
                }
            }
            else
            {
                uint32_t failedCount;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    ++mFailedCount;
                    mPassedCount = 0;
                    failedCount = mFailedCount;
                }
                if (failedCount >= mConfig.failedThreshold)
                {
                    updateStatus(DeadlineSupervisionStatus::kExpired);
                }
                else
                {
                    updateStatus(DeadlineSupervisionStatus::kFailed);
                }
            }
        }

        void DeadlineSupervision::updateStatus(DeadlineSupervisionStatus newStatus)
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
            if (changed && cb) cb(newStatus);
        }

    } // namespace phm
} // namespace ara
