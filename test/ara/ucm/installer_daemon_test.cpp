#include <gtest/gtest.h>
#include "../../../src/ara/ucm/installer_daemon.h"

namespace ara
{
    namespace ucm
    {
        TEST(InstallerDaemonTest, EnqueueTask)
        {
            InstallerDaemon _daemon;
            auto _result = _daemon.EnqueueTask("pkg_v1", "cluster_a");
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), "task_1");
        }

        TEST(InstallerDaemonTest, ProcessNextTask)
        {
            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg_v1", "cluster_a");
            auto _result = _daemon.ProcessNextTask();
            EXPECT_TRUE(_result.HasValue());

            auto _task = _daemon.GetTask("task_1");
            EXPECT_TRUE(_task.HasValue());
            EXPECT_EQ(_task.Value().State, InstallerTaskState::kCompleted);
        }

        TEST(InstallerDaemonTest, ProcessEmptyQueueFails)
        {
            InstallerDaemon _daemon;
            auto _result = _daemon.ProcessNextTask();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(InstallerDaemonTest, RollbackLastTask)
        {
            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg_v1", "cluster_a");
            _daemon.ProcessNextTask();
            auto _result = _daemon.RollbackLastTask();
            EXPECT_TRUE(_result.HasValue());

            auto _task = _daemon.GetTask("task_1");
            EXPECT_TRUE(_task.HasValue());
            EXPECT_EQ(_task.Value().State, InstallerTaskState::kRolledBack);
        }

        TEST(InstallerDaemonTest, GetAllTasks)
        {
            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.EnqueueTask("pkg2", "c2");
            auto _tasks = _daemon.GetAllTasks();
            EXPECT_EQ(_tasks.size(), 2U);
        }

        TEST(InstallerDaemonTest, ClearHistory)
        {
            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.ProcessNextTask();
            _daemon.ClearHistory();
            auto _tasks = _daemon.GetAllTasks();
            EXPECT_TRUE(_tasks.empty());
        }

        TEST(InstallerDaemonTest, TaskCallback)
        {
            InstallerDaemon _daemon;
            std::string _lastId;
            _daemon.SetTaskCallback([&](const InstallerTask &t) {
                _lastId = t.TaskId;
            });
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.ProcessNextTask();
            EXPECT_EQ(_lastId, "task_1");
        }

        TEST(InstallerDaemonTest, MultipleTasksSequential)
        {
            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.EnqueueTask("pkg2", "c2");
            _daemon.ProcessNextTask();
            _daemon.ProcessNextTask();

            auto _t1 = _daemon.GetTask("task_1");
            auto _t2 = _daemon.GetTask("task_2");
            EXPECT_EQ(_t1.Value().State, InstallerTaskState::kCompleted);
            EXPECT_EQ(_t2.Value().State, InstallerTaskState::kCompleted);
        }
    }
}
