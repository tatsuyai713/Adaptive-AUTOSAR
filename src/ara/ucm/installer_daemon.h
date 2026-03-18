/// @file src/ara/ucm/installer_daemon.h
/// @brief Background update installer daemon for UCM.
/// @details Manages scheduled update installation with state persistence,
///          progress tracking, and rollback support.

#ifndef INSTALLER_DAEMON_H
#define INSTALLER_DAEMON_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief Installer task state.
        enum class InstallerTaskState : uint8_t
        {
            kQueued = 0,
            kInProgress = 1,
            kCompleted = 2,
            kFailed = 3,
            kRolledBack = 4
        };

        /// @brief A single installer task (e.g. one package installation).
        struct InstallerTask
        {
            std::string TaskId;
            std::string PackageName;
            std::string TargetCluster;
            InstallerTaskState State{InstallerTaskState::kQueued};
            uint8_t ProgressPercent{0};
            std::string ErrorMessage;
        };

        /// @brief Callback for task state changes.
        using InstallerTaskCallback = std::function<void(
            const InstallerTask &task)>;

        /// @brief Background installer daemon for update management.
        /// @details Enqueues update tasks and processes them sequentially.
        class InstallerDaemon
        {
        public:
            InstallerDaemon();
            ~InstallerDaemon();

            InstallerDaemon(const InstallerDaemon &) = delete;
            InstallerDaemon &operator=(const InstallerDaemon &) = delete;

            /// @brief Enqueue an update task.
            core::Result<std::string> EnqueueTask(
                const std::string &packageName,
                const std::string &targetCluster);

            /// @brief Get the current task being processed (if any).
            InstallerTask GetCurrentTask() const;

            /// @brief Get the number of queued tasks.
            size_t QueueSize() const;

            /// @brief Get a task by its ID.
            core::Result<InstallerTask> GetTask(
                const std::string &taskId) const;

            /// @brief Process the next queued task synchronously.
            /// @details Moves it through kInProgress → kCompleted/kFailed.
            core::Result<void> ProcessNextTask();

            /// @brief Roll back the last completed task.
            core::Result<void> RollbackLastTask();

            /// @brief Set a callback for task state changes.
            void SetTaskCallback(InstallerTaskCallback callback);

            /// @brief Get all completed tasks.
            std::vector<InstallerTask> GetCompletedTasks() const;

            /// @brief Get all tasks (queued + in-progress + completed).
            std::vector<InstallerTask> GetAllTasks() const;

            /// @brief Clear completed and rolled-back tasks from history.
            void ClearHistory();

            /// @brief Check if any task is currently in progress.
            bool IsProcessing() const;

        private:
            mutable std::mutex mMutex;
            std::queue<InstallerTask> mQueue;
            std::vector<InstallerTask> mCompleted;
            InstallerTask mCurrentTask;
            bool mHasCurrentTask{false};
            uint32_t mNextTaskId{1};
            InstallerTaskCallback mCallback;

            void NotifyCallback(const InstallerTask &task);
        };
    }
}

#endif
