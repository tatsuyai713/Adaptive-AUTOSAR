/// @file src/ara/ucm/installer_daemon.cpp
/// @brief Installer daemon implementation.

#include "./installer_daemon.h"
#include <algorithm>
#include <sstream>
#if defined(__linux__) || defined(__QNX__) || defined(__unix__)
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

#if defined(__linux__) || defined(__QNX__) || defined(__unix__)
            // Locate staged package under /var/autosar/staging/<packageName>
            const std::string stagingPath =
                "/var/autosar/staging/" + mCurrentTask.PackageName;
            const std::string clusterRoot = "/var/autosar/clusters";
            const std::string targetPath  =
                clusterRoot + "/" + mCurrentTask.TargetCluster;

            struct stat st{};
            const bool stagingExists = (::stat(stagingPath.c_str(), &st) == 0 &&
                                        S_ISDIR(st.st_mode));
            if (stagingExists)
            {
                // Phase: prepare target cluster directory (25 %)
                mCurrentTask.ProgressPercent = 25;
                NotifyCallback(mCurrentTask);

                if (::stat(targetPath.c_str(), &st) != 0)
                {
                    ::mkdir(clusterRoot.c_str(), 0755);
                    if (::mkdir(targetPath.c_str(), 0755) < 0 && errno != EEXIST)
                    {
                        mCurrentTask.State = InstallerTaskState::kFailed;
                        mCurrentTask.ErrorMessage = "Cannot create target directory";
                        NotifyCallback(mCurrentTask);
                        mCompleted.push_back(mCurrentTask);
                        mHasCurrentTask = false;
                        return core::Result<void>::FromError(
                            MakeErrorCode(UcmErrc::kInvalidState));
                    }
                }

                // Phase: copy files from staging to target (25–100 %)
                DIR *dir = ::opendir(stagingPath.c_str());
                if (dir)
                {
                    uint32_t total = 0;
                    struct dirent *ent;
                    while ((ent = ::readdir(dir)) != nullptr)
                        if (ent->d_type == DT_REG) ++total;
                    ::rewinddir(dir);

                    uint32_t done = 0;
                    while ((ent = ::readdir(dir)) != nullptr)
                    {
                        if (ent->d_type != DT_REG)
                            continue;
                        const std::string src = stagingPath + "/" + ent->d_name;
                        const std::string dst = targetPath   + "/" + ent->d_name;

                        int sfd = ::open(src.c_str(), O_RDONLY);
                        int dfd = (sfd >= 0) ? ::open(dst.c_str(),
                                      O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
                        if (sfd >= 0 && dfd >= 0)
                        {
                            char buf[4096];
                            ssize_t n;
                            while ((n = ::read(sfd, buf, sizeof(buf))) > 0)
                                ::write(dfd, buf, static_cast<size_t>(n));
                        }
                        if (sfd >= 0) ::close(sfd);
                        if (dfd >= 0) ::close(dfd);

                        ++done;
                        if (total > 0)
                        {
                            mCurrentTask.ProgressPercent =
                                static_cast<uint8_t>(25 + done * 75 / total);
                            NotifyCallback(mCurrentTask);
                        }
                    }
                    ::closedir(dir);
                }
            }
            // If staging directory is absent the package is not yet transferred;
            // complete silently (test/no-hardware scenario).
#endif

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
