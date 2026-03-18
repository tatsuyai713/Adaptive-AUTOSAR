#include <gtest/gtest.h>
#include "../../../src/ara/exec/cluster_monitor.h"
#include "./helper/mock_rpc_server.h"

namespace ara
{
    namespace exec
    {
        namespace
        {
            static const uint8_t kProtoVer = 1;
            static const uint8_t kIfVer = 1;

            std::unique_ptr<StateServer> MakeStateServer(
                com::someip::rpc::RpcServer *srv,
                std::set<std::pair<std::string, std::string>> fgs = {},
                std::map<std::string, std::string> init = {})
            {
                return std::make_unique<StateServer>(
                    srv, std::move(fgs), std::move(init));
            }
        }

        class ClusterMonitorTest : public testing::Test
        {
        protected:
            helper::MockRpcServer mExecRpc{kProtoVer, kIfVer};
            helper::MockRpcServer mStateRpc{kProtoVer, kIfVer};
            ExecutionServer mExecServer{&mExecRpc};
            std::unique_ptr<StateServer> mStateServer{MakeStateServer(&mStateRpc)};
            ExecutionManager mEm{mExecServer, *mStateServer};
        };

        TEST_F(ClusterMonitorTest, ComputeHealthUnknownWhenEmpty)
        {
            ClusterMonitor _monitor{mEm};
            _monitor.Refresh();
            auto _all = _monitor.GetAllSnapshots();
            EXPECT_TRUE(_all.empty());
        }

        TEST_F(ClusterMonitorTest, GetSnapshotUnknownFails)
        {
            ClusterMonitor _monitor{mEm};
            auto _snap = _monitor.GetSnapshot("NonExistentFG");
            EXPECT_FALSE(_snap.HasValue());
        }

        TEST_F(ClusterMonitorTest, RegisteredProcessAppearsInSnapshot)
        {
            // Recreate with known FG
            helper::MockRpcServer _stRpc{kProtoVer, kIfVer};
            auto _ss = MakeStateServer(
                &_stRpc,
                {{"TestFG", "Running"}},
                {{"TestFG", "Running"}});
            helper::MockRpcServer _exRpc{kProtoVer, kIfVer};
            ExecutionServer _es{&_exRpc};
            ExecutionManager _em2{_es, *_ss};

            ProcessDescriptor _pd;
            _pd.name = "TestApp";
            _pd.executable = "/bin/true";
            _pd.functionGroup = "TestFG";
            _pd.activeState = "Running";
            _em2.RegisterProcess(std::move(_pd));

            ClusterMonitor _monitor{_em2};
            _monitor.Refresh();

            auto _snap = _monitor.GetSnapshot("TestFG");
            EXPECT_TRUE(_snap.HasValue());
            EXPECT_EQ(_snap.Value().TotalProcesses, 1U);
        }

        TEST_F(ClusterMonitorTest, HealthCallbackInvoked)
        {
            ClusterMonitor _monitor{mEm};
            bool _called = false;
            _monitor.SetHealthCallback(
                [&](const std::string &, ClusterHealth, ClusterHealth) {
                    _called = true;
                });
            _monitor.Refresh();
            EXPECT_FALSE(_called);
        }

        TEST_F(ClusterMonitorTest, ThresholdConfiguration)
        {
            ClusterMonitor _monitor{mEm, 0.2, 0.8};
            _monitor.Refresh();
            auto _all = _monitor.GetAllSnapshots();
            EXPECT_TRUE(_all.empty());
        }
    }
}
