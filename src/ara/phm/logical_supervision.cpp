/// @file src/ara/phm/logical_supervision.cpp
/// @brief Logical Supervision implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./logical_supervision.h"

namespace ara
{
    namespace phm
    {
        LogicalSupervision::LogicalSupervision(const LogicalSupervisionConfig &config)
            : mConfig{config},
              mStatus{LogicalSupervisionStatus::kDeactivated},
              mCurrentCheckpoint{config.initialCheckpoint},
              mInitialized{false}
        {
            buildAdjacency();
        }

        void LogicalSupervision::buildAdjacency()
        {
            mAdjacency.clear();
            for (const auto &t : mConfig.transitions)
            {
                mAdjacency[t.from].insert(t.to);
            }
        }

        void LogicalSupervision::Start()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCurrentCheckpoint = mConfig.initialCheckpoint;
            mInitialized = false;
            mFailedCount = 0;
            mPassedCount = 0;
            mStatus = LogicalSupervisionStatus::kOk;
        }

        void LogicalSupervision::Stop()
        {
            updateStatus(LogicalSupervisionStatus::kDeactivated);
        }

        void LogicalSupervision::ReportCheckpoint(CheckpointId checkpointId)
        {
            if (mStatus == LogicalSupervisionStatus::kDeactivated)
            {
                return;
            }

            bool passed = false;
            {
                std::lock_guard<std::mutex> lock(mMutex);

                if (!mInitialized)
                {
                    // First checkpoint must be the initial checkpoint
                    if (checkpointId == mConfig.initialCheckpoint)
                    {
                        mCurrentCheckpoint = checkpointId;
                        mInitialized = true;
                        passed = true;
                    }
                    else if (mConfig.allowReset && checkpointId == mConfig.initialCheckpoint)
                    {
                        mCurrentCheckpoint = checkpointId;
                        mInitialized = true;
                        passed = true;
                    }
                    else
                    {
                        // Unexpected first checkpoint
                        passed = false;
                        mInitialized = true;
                        mCurrentCheckpoint = checkpointId;
                    }
                }
                else
                {
                    // Check if the transition from current → checkpointId is valid
                    if (mConfig.allowReset && checkpointId == mConfig.initialCheckpoint)
                    {
                        // Allow reset to initial checkpoint from any state
                        mCurrentCheckpoint = checkpointId;
                        passed = true;
                    }
                    else
                    {
                        passed = isValidTransition(mCurrentCheckpoint, checkpointId);
                        mCurrentCheckpoint = checkpointId;
                    }
                }
            }

            recordResult(passed);
        }

        bool LogicalSupervision::isValidTransition(
            CheckpointId from, CheckpointId to) const noexcept
        {
            auto it = mAdjacency.find(from);
            if (it == mAdjacency.end()) return false;
            return it->second.count(to) > 0;
        }

        LogicalSupervisionStatus LogicalSupervision::GetStatus() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mStatus;
        }

        void LogicalSupervision::SetStatusCallback(StatusCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStatusCallback = std::move(callback);
        }

        std::set<CheckpointId> LogicalSupervision::GetValidSuccessors(
            CheckpointId checkpointId) const
        {
            auto it = mAdjacency.find(checkpointId);
            if (it == mAdjacency.end()) return {};
            return it->second;
        }

        void LogicalSupervision::recordResult(bool passed)
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
                    updateStatus(LogicalSupervisionStatus::kOk);
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
                    updateStatus(LogicalSupervisionStatus::kExpired);
                }
                else
                {
                    updateStatus(LogicalSupervisionStatus::kFailed);
                }
            }
        }

        void LogicalSupervision::updateStatus(LogicalSupervisionStatus newStatus)
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
