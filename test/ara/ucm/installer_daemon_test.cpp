#include <gtest/gtest.h>
#include "../../../src/ara/ucm/installer_daemon.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>

#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ara
{
    namespace ucm
    {
        namespace
        {
            std::string TestRoot()
            {
#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
                return "/tmp/ara_ucm_installer_daemon_test_" +
                       std::to_string(static_cast<long long>(::getpid()));
#else
                return "/tmp/ara_ucm_installer_daemon_test";
#endif
            }

            std::string StagingRoot()
            {
                return TestRoot() + "/staging";
            }

            std::string ClusterRoot()
            {
                return TestRoot() + "/clusters";
            }

#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
            void RemovePath(const std::string &path)
            {
                struct stat st{};
                if (::lstat(path.c_str(), &st) != 0)
                {
                    return;
                }

                if (S_ISDIR(st.st_mode))
                {
                    DIR *dir = ::opendir(path.c_str());
                    if (dir != nullptr)
                    {
                        struct dirent *ent;
                        while ((ent = ::readdir(dir)) != nullptr)
                        {
                            const std::string name{ent->d_name};
                            if (name == "." || name == "..")
                            {
                                continue;
                            }
                            RemovePath(path + "/" + name);
                        }
                        ::closedir(dir);
                    }
                    ::rmdir(path.c_str());
                }
                else
                {
                    ::unlink(path.c_str());
                }
            }

            bool EnsureDirectory(const std::string &path)
            {
                struct stat st{};
                if (::stat(path.c_str(), &st) == 0)
                {
                    return S_ISDIR(st.st_mode);
                }
                return ::mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
            }

            bool ConfigureStorageRoots()
            {
                const std::string root = TestRoot();
                if (!EnsureDirectory(root) ||
                    !EnsureDirectory(StagingRoot()) ||
                    !EnsureDirectory(ClusterRoot()))
                {
                    return false;
                }

                return ::setenv("ARA_UCM_STAGING_ROOT",
                                StagingRoot().c_str(),
                                1) == 0 &&
                       ::setenv("ARA_UCM_CLUSTER_ROOT",
                                ClusterRoot().c_str(),
                                1) == 0;
            }

            bool PrepareStagedPackage(const std::string &packageName,
                                      const std::string &targetCluster)
            {
                if (!ConfigureStorageRoots())
                {
                    return false;
                }

                RemovePath(StagingRoot() + "/" + packageName);
                RemovePath(ClusterRoot() + "/" + targetCluster);

                if (!EnsureDirectory(StagingRoot() + "/" + packageName))
                {
                    return false;
                }

                std::ofstream out{StagingRoot() + "/" + packageName + "/payload.bin",
                                  std::ios::binary};
                out << "payload";
                return out.good();
            }
#else
            bool PrepareStagedPackage(const std::string &,
                                      const std::string &)
            {
                return true;
            }
#endif
        }

        TEST(InstallerDaemonTest, EnqueueTask)
        {
            InstallerDaemon _daemon;
            auto _result = _daemon.EnqueueTask("pkg_v1", "cluster_a");
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), "task_1");
        }

        TEST(InstallerDaemonTest, EnqueueTaskRejectsPathTraversal)
        {
            InstallerDaemon _daemon;

            EXPECT_FALSE(_daemon.EnqueueTask("../pkg", "cluster_a").HasValue());
            EXPECT_FALSE(_daemon.EnqueueTask("pkg_v1", "../cluster").HasValue());
            EXPECT_FALSE(_daemon.EnqueueTask("pkg/v1", "cluster_a").HasValue());
            EXPECT_FALSE(_daemon.EnqueueTask("pkg_v1", "cluster/a").HasValue());
        }

        TEST(InstallerDaemonTest, ProcessNextTask)
        {
            if (!PrepareStagedPackage("pkg_v1", "cluster_a"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg_v1", "cluster_a");
            auto _result = _daemon.ProcessNextTask();
            EXPECT_TRUE(_result.HasValue());

            auto _task = _daemon.GetTask("task_1");
            EXPECT_TRUE(_task.HasValue());
            EXPECT_EQ(_task.Value().State, InstallerTaskState::kCompleted);
        }

        TEST(InstallerDaemonTest, MissingStagingPackageFails)
        {
#if defined(__linux__) || defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
            ASSERT_TRUE(ConfigureStorageRoots());
            RemovePath(StagingRoot() + "/pkg_missing_for_test");
            RemovePath(ClusterRoot() + "/cluster_missing");
#endif

            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg_missing_for_test", "cluster_missing");
            auto _result = _daemon.ProcessNextTask();
            EXPECT_FALSE(_result.HasValue());

            auto _task = _daemon.GetTask("task_1");
            ASSERT_TRUE(_task.HasValue());
            EXPECT_EQ(_task.Value().State, InstallerTaskState::kFailed);
            EXPECT_FALSE(_task.Value().ErrorMessage.empty());
        }

        TEST(InstallerDaemonTest, ProcessEmptyQueueFails)
        {
            InstallerDaemon _daemon;
            auto _result = _daemon.ProcessNextTask();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(InstallerDaemonTest, RollbackLastTask)
        {
            if (!PrepareStagedPackage("pkg_v1", "cluster_a"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

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
            if (!PrepareStagedPackage("pkg1", "c1"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.ProcessNextTask();
            _daemon.ClearHistory();
            auto _tasks = _daemon.GetAllTasks();
            EXPECT_TRUE(_tasks.empty());
        }

        TEST(InstallerDaemonTest, TaskCallback)
        {
            if (!PrepareStagedPackage("pkg1", "c1"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

            InstallerDaemon _daemon;
            std::string _lastId;
            _daemon.SetTaskCallback([&](const InstallerTask &t) {
                _lastId = t.TaskId;
            });
            _daemon.EnqueueTask("pkg1", "c1");
            _daemon.ProcessNextTask();
            EXPECT_EQ(_lastId, "task_1");
        }

        TEST(InstallerDaemonTest, TaskCallbackCanReenterDaemon)
        {
            if (!PrepareStagedPackage("pkg_reentrant", "c_reentrant"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

            InstallerDaemon _daemon;
            bool _callbackQueriedTasks{false};
            _daemon.SetTaskCallback([&](const InstallerTask &) {
                (void)_daemon.GetAllTasks();
                _callbackQueriedTasks = true;
            });

            _daemon.EnqueueTask("pkg_reentrant", "c_reentrant");
            auto _result = _daemon.ProcessNextTask();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_TRUE(_callbackQueriedTasks);
        }

        TEST(InstallerDaemonTest, MultipleTasksSequential)
        {
            if (!PrepareStagedPackage("pkg1", "c1") ||
                !PrepareStagedPackage("pkg2", "c2"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

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

        TEST(InstallerDaemonTest, CopiesStagedFileToTargetCluster)
        {
            if (!PrepareStagedPackage("pkg_copy", "cluster_copy"))
            {
                GTEST_SKIP() << "Cannot create temporary UCM staging paths";
            }

            InstallerDaemon _daemon;
            _daemon.EnqueueTask("pkg_copy", "cluster_copy");
            auto _result = _daemon.ProcessNextTask();
            ASSERT_TRUE(_result.HasValue());

            std::ifstream in{ClusterRoot() + "/cluster_copy/payload.bin",
                             std::ios::binary};
            std::string content;
            in >> content;
            EXPECT_EQ(content, "payload");
        }
    }
}
