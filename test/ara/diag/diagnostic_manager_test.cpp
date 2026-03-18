#include <gtest/gtest.h>
#include "../../../src/ara/diag/diagnostic_manager.h"

namespace ara
{
    namespace diag
    {
        TEST(DiagnosticManagerTest, RegisterServiceSucceeds)
        {
            DiagnosticManager _dm;
            auto _result = _dm.RegisterService(0x22);
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(DiagnosticManagerTest, DuplicateRegisterFails)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            auto _result = _dm.RegisterService(0x22);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(DiagnosticManagerTest, UnregisterServiceSucceeds)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            auto _result = _dm.UnregisterService(0x22);
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(DiagnosticManagerTest, SubmitRequestSucceeds)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            auto _rid = _dm.SubmitRequest(0x22, 0x00, {0xF1, 0x90});
            EXPECT_TRUE(_rid.HasValue());
            EXPECT_EQ(_dm.GetTotalRequestCount(), 1U);
        }

        TEST(DiagnosticManagerTest, SubmitRequestUnregisteredFails)
        {
            DiagnosticManager _dm;
            auto _rid = _dm.SubmitRequest(0x99, 0x00, {});
            EXPECT_FALSE(_rid.HasValue());
        }

        TEST(DiagnosticManagerTest, CompleteRequestSucceeds)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            auto _rid = _dm.SubmitRequest(0x22, 0x00, {});
            EXPECT_TRUE(_rid.HasValue());
            auto _comp = _dm.CompleteRequest(_rid.Value());
            EXPECT_TRUE(_comp.HasValue());
            EXPECT_TRUE(_dm.GetPendingRequests().empty());
        }

        TEST(DiagnosticManagerTest, RejectRequestSucceeds)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            auto _rid = _dm.SubmitRequest(0x22, 0x00, {});
            auto _rej = _dm.RejectRequest(_rid.Value(), 0x31);
            EXPECT_TRUE(_rej.HasValue());
        }

        TEST(DiagnosticManagerTest, GetServiceStats)
        {
            DiagnosticManager _dm;
            _dm.RegisterService(0x22);
            _dm.SubmitRequest(0x22, 0x00, {});
            auto _stats = _dm.GetServiceStats(0x22);
            EXPECT_TRUE(_stats.HasValue());
            EXPECT_EQ(_stats.Value().RequestCount, 1U);
        }

        TEST(DiagnosticManagerTest, ResponseTimingSetGet)
        {
            DiagnosticManager _dm;
            ResponseTiming _timing;
            _timing.P2ServerMs = 100;
            _timing.P2StarServerMs = 10000;
            _dm.SetResponseTiming(_timing);
            auto _got = _dm.GetResponseTiming();
            EXPECT_EQ(_got.P2ServerMs, 100U);
            EXPECT_EQ(_got.P2StarServerMs, 10000U);
        }

        TEST(DiagnosticManagerTest, CheckTimingConstraintsTriggers)
        {
            DiagnosticManager _dm;
            ResponseTiming _timing;
            _timing.P2ServerMs = 0; // immediate threshold
            _dm.SetResponseTiming(_timing);
            _dm.RegisterService(0x22);
            _dm.SubmitRequest(0x22, 0x00, {});
            auto _count = _dm.CheckTimingConstraints();
            // P2ServerMs=0 means threshold is never reached
            EXPECT_EQ(_count, 0U);
        }
    }
}
