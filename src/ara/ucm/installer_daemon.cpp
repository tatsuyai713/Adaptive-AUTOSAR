/// @file src/ara/ucm/installer_daemon.cpp
/// @brief Installer daemon implementation.

#include "./installer_daemon.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
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

#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
        namespace
        {
            bool WriteAll(int fd, const char *buffer, size_t size)
            {
                size_t written = 0;
                while (written < size)
                {
                    ssize_t n = ::write(fd, buffer + written, size - written);
                    if (n <= 0)
                    {
                        return false;
                    }
                    written += static_cast<size_t>(n);
                }
                return true;
            }

            std::string GetEnvironmentOrDefault(const char *name,
                                                const char *defaultValue)
            {
                const char *value = std::getenv(name);
                if (value == nullptr || value[0] == '\0')
                {
                    return defaultValue;
                }
                return value;
            }
        }
#endif

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

            auto failTask = [&](const std::string &message) {
                mCurrentTask.State = InstallerTaskState::kFailed;
                mCurrentTask.ErrorMessage = message;
                NotifyCallback(mCurrentTask);
                mCompleted.push_back(mCurrentTask);
                mHasCurrentTask = false;
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidState));
            };

#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
            // Locate staged package under /var/autosar/staging/<packageName>
            const std::string stagingRoot = GetEnvironmentOrDefault(
                "ARA_UCM_STAGING_ROOT", "/var/autosar/staging");
            const std::string stagingPath =
                stagingRoot + "/" + mCurrentTask.PackageName;
            const std::string clusterRoot = GetEnvironmentOrDefault(
                "ARA_UCM_CLUSTER_ROOT", "/var/autosar/clusters");
            const std::string targetPath  =
                clusterRoot + "/" + mCurrentTask.TargetCluster;

            struct stat st{};
            const bool stagingExists = (::stat(stagingPath.c_str(), &st) == 0 &&
                                        S_ISDIR(st.st_mode));
            if (!stagingExists)
            {
                return failTask("Staging package path is absent");
            }

            // Phase: prepare target cluster directory (25 %)
            mCurrentTask.ProgressPercent = 25;
            NotifyCallback(mCurrentTask);

            if (::stat(clusterRoot.c_str(), &st) != 0)
            {
                if (::mkdir(clusterRoot.c_str(), 0755) < 0 && errno != EEXIST)
                {
                    return failTask("Cannot create cluster root directory");
                }
            }
            else if (!S_ISDIR(st.st_mode))
            {
                return failTask("Cluster root path is not a directory");
            }

            if (::stat(targetPath.c_str(), &st) != 0)
            {
                if (::mkdir(targetPath.c_str(), 0755) < 0 && errno != EEXIST)
                {
                    return failTask("Cannot create target directory");
                }
            }
            else if (!S_ISDIR(st.st_mode))
            {
                return failTask("Target path is not a directory");
            }

            // Phase: copy files from staging to target (25-100 %)
            DIR *dir = ::opendir(stagingPath.c_str());
            if (dir == nullptr)
            {
                return failTask("Cannot open staging directory");
            }

            auto isRegularFile = [&](const char *name) -> bool {
                const std::string full = stagingPath + "/" + name;
                struct stat fileStat{};
                if (::stat(full.c_str(), &fileStat) != 0)
                {
                    return false;
                }
                return S_ISREG(fileStat.st_mode);
            };

            uint32_t total = 0;
            struct dirent *ent;
            while ((ent = ::readdir(dir)) != nullptr)
            {
                if (isRegularFile(ent->d_name))
                {
                    ++total;
                }
            }
            ::rewinddir(dir);

            uint32_t done = 0;
            while ((ent = ::readdir(dir)) != nullptr)
            {
                if (!isRegularFile(ent->d_name))
                {
                    continue;
                }

                const std::string src = stagingPath + "/" + ent->d_name;
                const std::string dst = targetPath + "/" + ent->d_name;

                int sfd = ::open(src.c_str(), O_RDONLY);
                if (sfd < 0)
                {
                    ::closedir(dir);
                    return failTask("Cannot open staged file for reading");
                }

                int dfd = ::open(dst.c_str(),
                                 O_WRONLY | O_CREAT | O_TRUNC,
                                 0644);
                if (dfd < 0)
                {
                    const bool closeOk = (::close(sfd) == 0);
                    ::closedir(dir);
                    return failTask(closeOk
                                        ? "Cannot open target file for writing"
                                        : "Cannot close staged file");
                }

                char buf[4096];
                ssize_t n = 0;
                bool copyOk = true;
                while ((n = ::read(sfd, buf, sizeof(buf))) > 0)
                {
                    if (!WriteAll(dfd, buf, static_cast<size_t>(n)))
                    {
                        copyOk = false;
                        break;
                    }
                }
                if (n < 0)
                {
                    copyOk = false;
                }

                const bool sourceCloseOk = (::close(sfd) == 0);
                const bool targetCloseOk = (::close(dfd) == 0);

                if (!copyOk || !sourceCloseOk || !targetCloseOk)
                {
                    ::closedir(dir);
                    if (!copyOk)
                    {
                        return failTask("Cannot copy staged file");
                    }
                    if (!sourceCloseOk)
                    {
                        return failTask("Cannot close staged file");
                    }
                    return failTask("Cannot close target file");
                }

                ++done;
                if (total > 0)
                {
                    mCurrentTask.ProgressPercent =
                        static_cast<uint8_t>(25 + done * 75 / total);
                    NotifyCallback(mCurrentTask);
                }
            }

            if (::closedir(dir) != 0)
            {
                return failTask("Cannot close staging directory");
            }
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
