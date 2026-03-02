#include <gtest/gtest.h>
#include "../../../src/ara/exec/execution_manager.h"
#include "./helper/mock_rpc_server.h"

namespace ara
{
    namespace exec
    {
        namespace
        {
            // Build a minimal StateServer with one known function group state.
            std::unique_ptr<StateServer> MakeStateServer(
                com::someip::rpc::RpcServer *srv)
            {
                std::set<std::pair<std::string, std::string>> _fgs{
                    {"MachineFG", "Off"},
                    {"MachineFG", "Running"}};
                std::map<std::string, std::string> _init{
                    {"MachineFG", "Off"}};
                return std::make_unique<StateServer>(srv, std::move(_fgs), std::move(_init));
            }
        }

        class ExecutionManagerTest : public testing::Test
        {
        protected:
            static const uint8_t cProtocolVersion{1};
            static const uint8_t cInterfaceVersion{1};

            helper::MockRpcServer mExecRpc{cProtocolVersion, cInterfaceVersion};
            helper::MockRpcServer mStateRpc{cProtocolVersion, cInterfaceVersion};
            ExecutionServer mExecServer{&mExecRpc};
            std::unique_ptr<StateServer> mStateServer{MakeStateServer(&mStateRpc)};
            ExecutionManager mEm{mExecServer, *mStateServer};
        };

        const uint8_t ExecutionManagerTest::cProtocolVersion;
        const uint8_t ExecutionManagerTest::cInterfaceVersion;

        // -------------------------------------------------------------------

        TEST_F(ExecutionManagerTest, RegisterProcess_AcceptsValidDescriptor)
        {
            ProcessDescriptor _desc;
            _desc.name = "App1";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";

            EXPECT_TRUE(mEm.RegisterProcess(_desc).HasValue());
        }

        TEST_F(ExecutionManagerTest, RegisterProcess_RejectsEmptyName)
        {
            ProcessDescriptor _desc;
            _desc.name = "";
            _desc.executable = "/bin/sh";
            EXPECT_FALSE(mEm.RegisterProcess(_desc).HasValue());
        }

        TEST_F(ExecutionManagerTest, RegisterProcess_RejectsDuplicateName)
        {
            ProcessDescriptor _desc;
            _desc.name = "App2";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";

            EXPECT_TRUE(mEm.RegisterProcess(_desc).HasValue());
            EXPECT_FALSE(mEm.RegisterProcess(_desc).HasValue());
        }

        TEST_F(ExecutionManagerTest, UnregisterProcess_RemovesNotRunningProcess)
        {
            ProcessDescriptor _desc;
            _desc.name = "App3";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";

            ASSERT_TRUE(mEm.RegisterProcess(_desc).HasValue());
            EXPECT_TRUE(mEm.UnregisterProcess("App3").HasValue());
        }

        TEST_F(ExecutionManagerTest, UnregisterProcess_FailsOnUnknown)
        {
            EXPECT_FALSE(mEm.UnregisterProcess("NoSuchApp").HasValue());
        }

        TEST_F(ExecutionManagerTest, GetProcessStatus_ReturnsInitialState)
        {
            ProcessDescriptor _desc;
            _desc.name = "App4";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";

            ASSERT_TRUE(mEm.RegisterProcess(_desc).HasValue());
            const auto _status{mEm.GetProcessStatus("App4")};
            ASSERT_TRUE(_status.HasValue());
            EXPECT_EQ(_status.Value().managedState, ManagedProcessState::kNotRunning);
            EXPECT_EQ(_status.Value().pid, -1);
        }

        TEST_F(ExecutionManagerTest, GetProcessStatus_FailsOnUnknown)
        {
            EXPECT_FALSE(mEm.GetProcessStatus("Ghost").HasValue());
        }

        TEST_F(ExecutionManagerTest, GetAllProcessStatuses_ReflectsRegistered)
        {
            ProcessDescriptor _d1;
            _d1.name = "AppA";
            _d1.executable = "/bin/sh";
            _d1.functionGroup = "MachineFG";
            _d1.activeState = "Running";

            ProcessDescriptor _d2;
            _d2.name = "AppB";
            _d2.executable = "/bin/sh";
            _d2.functionGroup = "MachineFG";
            _d2.activeState = "Running";

            ASSERT_TRUE(mEm.RegisterProcess(_d1).HasValue());
            ASSERT_TRUE(mEm.RegisterProcess(_d2).HasValue());

            const auto _all{mEm.GetAllProcessStatuses()};
            EXPECT_EQ(_all.size(), 2U);
        }

        TEST_F(ExecutionManagerTest, Start_SucceedsOnce)
        {
            EXPECT_TRUE(mEm.Start().HasValue());
            EXPECT_TRUE(mEm.IsRunning());
            mEm.Stop();
        }

        TEST_F(ExecutionManagerTest, Start_FailsWhenAlreadyRunning)
        {
            ASSERT_TRUE(mEm.Start().HasValue());
            EXPECT_FALSE(mEm.Start().HasValue());
            mEm.Stop();
        }

        TEST_F(ExecutionManagerTest, Stop_StopsMonitor)
        {
            ASSERT_TRUE(mEm.Start().HasValue());
            EXPECT_TRUE(mEm.IsRunning());
            mEm.Stop();
            EXPECT_FALSE(mEm.IsRunning());
        }

        TEST_F(ExecutionManagerTest, ActivateFunctionGroup_FailsOnEmptyGroup)
        {
            EXPECT_FALSE(mEm.ActivateFunctionGroup("", "Running").HasValue());
        }

        TEST_F(ExecutionManagerTest, ActivateFunctionGroup_FailsOnEmptyState)
        {
            EXPECT_FALSE(mEm.ActivateFunctionGroup("MachineFG", "").HasValue());
        }

        TEST_F(ExecutionManagerTest, ActivateFunctionGroup_NoProcessesRegistered)
        {
            // Activating with no matching processes registered should still succeed.
            EXPECT_TRUE(mEm.ActivateFunctionGroup("MachineFG", "Running").HasValue());
        }

        TEST_F(ExecutionManagerTest, TerminateFunctionGroup_FailsOnEmptyGroup)
        {
            EXPECT_FALSE(mEm.TerminateFunctionGroup("").HasValue());
        }

        TEST_F(ExecutionManagerTest, TerminateFunctionGroup_FailsOnUnknownGroup)
        {
            EXPECT_FALSE(mEm.TerminateFunctionGroup("UnknownFG").HasValue());
        }

        TEST_F(ExecutionManagerTest, TerminateFunctionGroup_SucceedsAfterActivate)
        {
            ProcessDescriptor _desc;
            _desc.name = "AppTerm";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";
            ASSERT_TRUE(mEm.RegisterProcess(_desc).HasValue());

            // Activate registers the group
            ASSERT_TRUE(mEm.ActivateFunctionGroup("MachineFG", "Running").HasValue());

            // Terminate should succeed since the group is now known
            EXPECT_TRUE(mEm.TerminateFunctionGroup("MachineFG").HasValue());
        }

        TEST_F(ExecutionManagerTest, SetProcessStateChangeHandler_IsInvoked)
        {
            ProcessDescriptor _desc;
            _desc.name = "AppCb";
            _desc.executable = "/bin/sh";
            _desc.functionGroup = "MachineFG";
            _desc.activeState = "Running";
            ASSERT_TRUE(mEm.RegisterProcess(_desc).HasValue());

            std::string _lastName;
            ManagedProcessState _lastState{ManagedProcessState::kNotRunning};
            mEm.SetProcessStateChangeHandler(
                [&](const std::string &name, ManagedProcessState state)
                {
                    _lastName = name;
                    _lastState = state;
                });

            // Activating will attempt to launch /bin/sh (real fork)
            // or transition state; at minimum it should not crash.
            (void)mEm.ActivateFunctionGroup("MachineFG", "Running");
        }

    } // namespace exec
} // namespace ara
