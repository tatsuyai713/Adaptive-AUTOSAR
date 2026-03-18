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

            // --- New tests for replay protection, SendSyncRequest, and RemoteCounter persistence ---

            TEST(FreshnessSyncManagerTest, VerifyFreshnessRejectsExactReplay)
            {
                // After accepting a counter, the same counter must be rejected (replay).
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 32;
                _wc.BackwardTolerance = 0;
                _mgr.RegisterPdu(1, _wc);

                // Accept counter 5
                EXPECT_EQ(_mgr.VerifyFreshness(1, 5), FreshnessVerifyResult::kAccepted);
                // Replay of counter 5 must be rejected
                EXPECT_EQ(_mgr.VerifyFreshness(1, 5), FreshnessVerifyResult::kRejectedTooOld);
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessRejectsSmallerAfterAdvance)
            {
                // After accepting counter N, counters < N must be rejected with BT=0.
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 32;
                _wc.BackwardTolerance = 0;
                _mgr.RegisterPdu(1, _wc);

                EXPECT_EQ(_mgr.VerifyFreshness(1, 10), FreshnessVerifyResult::kAccepted);
                EXPECT_EQ(_mgr.VerifyFreshness(1,  9), FreshnessVerifyResult::kRejectedTooOld);
                EXPECT_EQ(_mgr.VerifyFreshness(1,  1), FreshnessVerifyResult::kRejectedTooOld);
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessMonotonicAdvance)
            {
                // Counters accepted in order should each advance the window.
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 32;
                _wc.BackwardTolerance = 0;
                _mgr.RegisterPdu(1, _wc);

                for (uint64_t i = 1; i <= 10; ++i)
                {
                    EXPECT_EQ(_mgr.VerifyFreshness(1, i), FreshnessVerifyResult::kAccepted);
                }
                // After 10, counter=5 is stale
                EXPECT_EQ(_mgr.VerifyFreshness(1, 5), FreshnessVerifyResult::kRejectedTooOld);
            }

            TEST(FreshnessSyncManagerTest, VerifyFreshnessBackwardToleranceAcceptsOutOfOrder)
            {
                // With BackwardTolerance=2, counters up to 2 steps behind are accepted.
                FreshnessSyncManager _mgr{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 32;
                _wc.BackwardTolerance = 2;
                _mgr.RegisterPdu(1, _wc);

                EXPECT_EQ(_mgr.VerifyFreshness(1, 10), FreshnessVerifyResult::kAccepted);
                // Out-of-order: 9 and 8 are within tolerance
                EXPECT_EQ(_mgr.VerifyFreshness(1,  9), FreshnessVerifyResult::kAccepted);
                EXPECT_EQ(_mgr.VerifyFreshness(1,  8), FreshnessVerifyResult::kAccepted);
                // 7 is 3 steps behind — outside tolerance
                EXPECT_EQ(_mgr.VerifyFreshness(1,  7), FreshnessVerifyResult::kRejectedTooOld);
            }

            TEST(FreshnessSyncManagerTest, SendSyncRequestInvokesSenderCallback)
            {
                FreshnessSyncManager _mgr{"ECU_A"};
                _mgr.RegisterPdu(3, ReplayWindowConfig{});
                _mgr.IncrementCounter(3);

                bool _called = false;
                FreshnessSyncRequest _captured{};
                _mgr.SetSyncMessageSender([&](const FreshnessSyncRequest &req) {
                    _called = true;
                    _captured = req;
                });

                auto _result = _mgr.SendSyncRequest(3);
                EXPECT_TRUE(_result.HasValue());
                EXPECT_TRUE(_called);
                EXPECT_EQ(_captured.Pdu, 3);
                EXPECT_EQ(_captured.CounterValue, 1U);
                EXPECT_EQ(_captured.SourceEcuId, "ECU_A");
            }

            TEST(FreshnessSyncManagerTest, SendSyncRequestFailsWithoutSender)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});
                // No sender registered
                auto _result = _mgr.SendSyncRequest(1);
                EXPECT_FALSE(_result.HasValue());
            }

            TEST(FreshnessSyncManagerTest, SendSyncRequestFailsForUnknownPdu)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.SetSyncMessageSender([](const FreshnessSyncRequest &) {});
                auto _result = _mgr.SendSyncRequest(99);
                EXPECT_FALSE(_result.HasValue());
            }

            TEST(FreshnessSyncManagerTest, PersistAndRestoreRemoteCounter)
            {
                // RemoteCounter must be preserved across persist/restore so that
                // replay protection survives a restart.
                FreshnessSyncManager _mgr1{"TestEcu"};
                ReplayWindowConfig _wc;
                _wc.ForwardTolerance = 32;
                _wc.BackwardTolerance = 0;
                _mgr1.RegisterPdu(2, _wc);

                // Accept counter 7 — RemoteCounter advances to 7.
                EXPECT_EQ(_mgr1.VerifyFreshness(2, 7), FreshnessVerifyResult::kAccepted);

                auto _buf = _mgr1.PersistState();

                // Restore into a new manager.
                FreshnessSyncManager _mgr2{"TestEcu"};
                _mgr2.RegisterPdu(2, _wc);
                auto _restoreResult = _mgr2.RestoreState(_buf);
                EXPECT_TRUE(_restoreResult.HasValue());

                // Counter 7 must be rejected as a replay after restore.
                EXPECT_EQ(_mgr2.VerifyFreshness(2, 7), FreshnessVerifyResult::kRejectedTooOld);
                // Counter 8 must be accepted.
                EXPECT_EQ(_mgr2.VerifyFreshness(2, 8), FreshnessVerifyResult::kAccepted);
            }

            TEST(FreshnessSyncManagerTest, CreateSyncRequestReturnsLocalCounter)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(5, ReplayWindowConfig{});
                _mgr.IncrementCounter(5);
                _mgr.IncrementCounter(5);
                _mgr.IncrementCounter(5);

                auto _req = _mgr.CreateSyncRequest(5);
                EXPECT_TRUE(_req.HasValue());
                EXPECT_EQ(_req.Value().CounterValue, 3U);
                EXPECT_EQ(_req.Value().Pdu, 5);
                EXPECT_EQ(_req.Value().SourceEcuId, "TestEcu");
            }

            TEST(FreshnessSyncManagerTest, GetLocalEcuId)
            {
                FreshnessSyncManager _mgr{"MyECU"};
                EXPECT_EQ(_mgr.GetLocalEcuId(), "MyECU");
            }

            TEST(FreshnessSyncManagerTest, RestoreStateInvalidDataFails)
            {
                FreshnessSyncManager _mgr{"TestEcu"};
                _mgr.RegisterPdu(1, ReplayWindowConfig{});
                // Too short — must fail
                std::vector<uint8_t> _bad = {0x01, 0x00, 0x00};
                EXPECT_FALSE(_mgr.RestoreState(_bad).HasValue());
            }
        }
    }
}
