#include <gtest/gtest.h>
#include "../../../../src/ara/com/secoc/freshness_sync_manager.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            TEST(FreshnessSyncManagerTest, RegisterPduSucceeds)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                auto _result = _mgr.RegisterPdu(1, ReplayWindowConfig{});
                EXPECT_TRUE(_result.HasValue());
                EXPECT_EQ(_mgr.RegisteredPduCount(), 1U);
            }

            TEST(FreshnessSyncManagerTest, DuplicateRegisterFails)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});
                auto _result = _mgr.RegisterPdu(1, ReplayWindowConfig{});
                EXPECT_FALSE(_result.HasValue());
            }

            TEST(FreshnessSyncManagerTest, IncrementCounterWorks)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});
                auto _c1 = _mgr.IncrementCounter(1);
                EXPECT_TRUE(_c1.HasValue());
                EXPECT_EQ(_c1.Value(), 1U);
                auto _c2 = _mgr.IncrementCounter(1);
                EXPECT_EQ(_c2.Value(), 2U);
            }

            TEST(FreshnessSyncManagerTest, GetLocalCounterUnknownPduFails)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                auto _result = _mgr.GetLocalCounter(99);
                EXPECT_FALSE(_result.HasValue());
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessAccepted)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 16;
                _mgr.RegisterPdu(1, _wc);
                auto _result = _mgr.VerifyFreshness(1, 0);
                EXPECT_EQ(_result, FreshnessVerifyResult::kAccepted);
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessRejectsTooNew)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 2;
                _mgr.RegisterPdu(1, _wc);
                auto _result = _mgr.VerifyFreshness(1, 100);
                EXPECT_EQ(_result, FreshnessVerifyResult::kRejectedTooNew);
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessUnknownPdu)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                auto _result = _mgr.VerifyFreshness(99, 0);
                EXPECT_EQ(_result, FreshnessVerifyResult::kRejectedUnknownPdu);
            }

            TEST(FreshnessSyncManagerTest, SyncStateInitiallyUnsynchronized)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});
                EXPECT_EQ(_mgr.GetSyncState(1),
                          FreshnessSyncState::kUnsynchronized);
            }

            TEST(FreshnessSyncManagerTest, ProcessSyncResponseUpdatesSyncState)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});

                FreshnessSyncResponse _resp;
                _resp.Pdu = 1;
                _resp.AcknowledgedCounter = 42;
                _resp.SyncState = FreshnessSyncState::kSynchronized;
                _resp.ResponderEcuId = "RemoteEcu";
                auto _result = _mgr.ProcessSyncResponse(_resp);
                EXPECT_TRUE(_result.HasValue());
                EXPECT_EQ(_mgr.GetSyncState(1),
                          FreshnessSyncState::kSynchronized);
            }

            TEST(FreshnessSyncManagerTest, PersistAndRestoreState)
            {
                FreshnessSyncManager _mgr1{"TestEcu"};
                _mgr1.RegisterPdu(1, ReplayWindowConfig{});
                _mgr1.IncrementCounter(1);
                _mgr1.IncrementCounter(1);

                auto _buf = _mgr1.PersistState();
                EXPECT_FALSE(_buf.empty());

                FreshnessSyncManager _mgr2{"TestEcu"};
                _mgr2.RegisterPdu(1, ReplayWindowConfig{});
                auto _result = _mgr2.RestoreState(_buf);
                EXPECT_TRUE(_result.HasValue());
                auto _counter = _mgr2.GetLocalCounter(1);
                EXPECT_TRUE(_counter.HasValue());
                EXPECT_EQ(_counter.Value(), 2U);
            }
        }
    }
}
