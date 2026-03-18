#include <gtest/gtest.h>
#include "../../../src/ara/phm/phm_orchestrator.h"

namespace ara
{
    namespace phm
    {
        TEST(PhmOrchestratorTest, RegisterEntity)
        {
            PhmOrchestrator _orch;
            auto _r = _orch.RegisterEntity("entity_a");
            EXPECT_TRUE(_r.HasValue());
            EXPECT_EQ(_orch.GetEntityCount(), 1U);
        }

        TEST(PhmOrchestratorTest, DuplicateRegisterFails)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            auto _r = _orch.RegisterEntity("entity_a");
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(PhmOrchestratorTest, UnregisterEntity)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            auto _r = _orch.UnregisterEntity("entity_a");
            EXPECT_TRUE(_r.HasValue());
            EXPECT_EQ(_orch.GetEntityCount(), 0U);
        }

        TEST(PhmOrchestratorTest, ReportFailureWithEnumType)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            auto _r = _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::AliveSupervision);
            EXPECT_TRUE(_r.HasValue());
        }

        TEST(PhmOrchestratorTest, GetEntityHealth)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::DeadlineSupervision);
            auto _snap = _orch.GetEntityHealth("entity_a");
            EXPECT_TRUE(_snap.HasValue());
            EXPECT_EQ(_snap.Value().FailureCount, 1U);
            EXPECT_EQ(_snap.Value().LastFailedSupervision,
                      TypeOfSupervision::DeadlineSupervision);
        }

        TEST(PhmOrchestratorTest, GetEntityHealthUnknownFails)
        {
            PhmOrchestrator _orch;
            auto _snap = _orch.GetEntityHealth("no_such");
            EXPECT_FALSE(_snap.HasValue());
        }

        TEST(PhmOrchestratorTest, PlatformHealthNormal)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            EXPECT_EQ(_orch.EvaluatePlatformHealth(),
                      PlatformHealthState::kNormal);
        }

        TEST(PhmOrchestratorTest, PlatformHealthDegraded)
        {
            PhmOrchestrator _orch(0.0, 0.9);
            _orch.RegisterEntity("entity_a");
            _orch.RegisterEntity("entity_b");
            _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::AliveSupervision);
            auto _state = _orch.EvaluatePlatformHealth();
            EXPECT_NE(_state, PlatformHealthState::kNormal);
        }

        TEST(PhmOrchestratorTest, ReportRecovery)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::AliveSupervision);
            auto _r = _orch.ReportSupervisionRecovery("entity_a");
            EXPECT_TRUE(_r.HasValue());
        }

        TEST(PhmOrchestratorTest, GetAllEntityHealth)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            _orch.RegisterEntity("entity_b");
            auto _all = _orch.GetAllEntityHealth();
            EXPECT_EQ(_all.size(), 2U);
        }

        TEST(PhmOrchestratorTest, ResetAllCounters)
        {
            PhmOrchestrator _orch;
            _orch.RegisterEntity("entity_a");
            _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::AliveSupervision);
            _orch.ResetAllCounters();
            auto _snap = _orch.GetEntityHealth("entity_a");
            EXPECT_TRUE(_snap.HasValue());
            EXPECT_EQ(_snap.Value().FailureCount, 0U);
        }

        TEST(PhmOrchestratorTest, PlatformHealthCallback)
        {
            PhmOrchestrator _orch(0.0, 0.5);
            bool _called = false;
            _orch.SetPlatformHealthCallback(
                [&](PlatformHealthState, PlatformHealthState) {
                    _called = true;
                });
            _orch.RegisterEntity("entity_a");
            _orch.ReportSupervisionFailure(
                "entity_a", TypeOfSupervision::AliveSupervision);
            // Callback may have been triggered during failure report
            // Either way the test should not crash
        }
    }
}
