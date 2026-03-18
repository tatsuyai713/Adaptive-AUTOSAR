/// @file src/ara/ucm/installer_daemon.cpp
/// @brief Installer daemon implementation.

#include "./installer_daemon.h"
#include <algorithm>
#include <sstream>

namespace ara
{
    namespace ucm
    {
        InstallerDaemon::InstallerDaemon() = default;
        InstallerDaemon::~InstallerDaemon() = default;

        core::Result<std::string> InstallerDaemon::EnqueueTask(
            const std::string &packageName,
            const std::string &targetCluster)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            InstallerTask task;
            std::ostringstream oss;
            oss << "task_" << mNextTaskId++;
            task.TaskId = oss.str();
            task.PackageName = packageName;
            task.TargetCluster = targetCluster;
            task.State = InstallerTaskState::kQueued;

            mQueue.push(task);
            return core::Result<std::string>::FromValue(task.TaskId);
        }

        InstallerTask InstallerDaemon::GetCurrentTask() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mHasCurrentTask)
                return mCurrentTask;
            return InstallerTask{};
        }

        size_t InstallerDaemon::QueueSize() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mQueue.size();
        }

        core::Result<InstallerTask> InstallerDaemon::GetTask(
            const std::string &taskId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mHasCurrentTask && mCurrentTask.TaskId == taskId)
            {
                return core::Result<InstallerTask>::FromValue(mCurrentTask);
            }

            for (const auto &t : mCompleted)
            {
                if (t.TaskId == taskId)
                {
                    return core::Result<InstallerTask>::FromValue(t);
                }
            }

            // Search queue (copy to iterate)
            auto qcopy = mQueue;
            while (!qcopy.empty())
            {
                if (qcopy.front().TaskId == taskId)
                {
                    return core::Result<InstallerTask>::FromValue(
                        qcopy.front());
                }
                qcopy.pop();
            }

            return core::Result<InstallerTask>::FromError(
                MakeErrorCode(UcmErrc::kInvalidArgument));
        }

        core::Result<void> InstallerDaemon::ProcessNextTask()
        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mQueue.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidState));
            }

            mCurrentTask = mQueue.front();
            mQueue.pop();
            mHasCurrentTask = true;

            mCurrentTask.State = InstallerTaskState::kInProgress;
            mCurrentTask.ProgressPercent = 0;
            NotifyCallback(mCurrentTask);

            // Simulate installation phases
            mCurrentTask.ProgressPercent = 50;
            mCurrentTask.ProgressPercent = 100;
            mCurrentTask.State = InstallerTaskState::kCompleted;
            NotifyCallback(mCurrentTask);

            mCompleted.push_back(mCurrentTask);
            mHasCurrentTask = false;

            return core::Result<void>::FromValue();
        }

        core::Result<void> InstallerDaemon::RollbackLastTask()
        {
            std::lock_guard<std::mutex> lock(mMutex);

            if (mCompleted.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidState));
            }

            auto &last = mCompleted.back();
            if (last.State != InstallerTaskState::kCompleted)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidState));
            }

            last.State = InstallerTaskState::kRolledBack;
            NotifyCallback(last);

            return core::Result<void>::FromValue();
        }

        void InstallerDaemon::SetTaskCallback(
            InstallerTaskCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(callback);
        }

        std::vector<InstallerTask> InstallerDaemon::GetCompletedTasks() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCompleted;
        }

        std::vector<InstallerTask> InstallerDaemon::GetAllTasks() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<InstallerTask> all;

            auto qcopy = mQueue;
            while (!qcopy.empty())
            {
                all.push_back(qcopy.front());
                qcopy.pop();
            }

            if (mHasCurrentTask)
                all.push_back(mCurrentTask);

            all.insert(all.end(), mCompleted.begin(), mCompleted.end());
            return all;
        }

        void InstallerDaemon::ClearHistory()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCompleted.erase(
                std::remove_if(mCompleted.begin(), mCompleted.end(),
                               [](const InstallerTask &t)
                               {
                                   return t.State == InstallerTaskState::kCompleted ||
                                          t.State == InstallerTaskState::kRolledBack;
                               }),
                mCompleted.end());
        }

        bool InstallerDaemon::IsProcessing() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mHasCurrentTask;
        }

        void InstallerDaemon::NotifyCallback(const InstallerTask &task)
        {
            if (mCallback)
                mCallback(task);
        }
    }
}
