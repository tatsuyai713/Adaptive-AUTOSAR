#include <gtest/gtest.h>
#include "../../../src/ara/exec/application_client.h"

namespace ara
{
    namespace exec
    {
        static const core::InstanceSpecifier cSpecifier("TestApp");

        TEST(ApplicationClientTest, InitialRecoveryActionIsNone)
        {
            ApplicationClient _client(cSpecifier);
            auto _result = _client.GetRecoveryAction();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), ApplicationRecoveryAction::kNone);
        }

        TEST(ApplicationClientTest, RequestRecoveryActionSetsAction)
        {
            ApplicationClient _client(cSpecifier);
            _client.RequestRecoveryAction(ApplicationRecoveryAction::kRestart);

            auto _result = _client.GetRecoveryAction();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), ApplicationRecoveryAction::kRestart);
        }

        TEST(ApplicationClientTest, AcknowledgeClearsAction)
        {
            ApplicationClient _client(cSpecifier);
            _client.RequestRecoveryAction(ApplicationRecoveryAction::kTerminate);
            ASSERT_TRUE(_client.AcknowledgeRecoveryAction().HasValue());

            auto _result = _client.GetRecoveryAction();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), ApplicationRecoveryAction::kNone);
        }

        TEST(ApplicationClientTest, AcknowledgeWithoutActionFails)
        {
            ApplicationClient _client(cSpecifier);
            auto _result = _client.AcknowledgeRecoveryAction();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(ApplicationClientTest, HealthReportIncrements)
        {
            ApplicationClient _client(cSpecifier);
            EXPECT_EQ(_client.GetHealthReportCount(), 0U);

            _client.ReportApplicationHealth();
            _client.ReportApplicationHealth();
            _client.ReportApplicationHealth();

            EXPECT_EQ(_client.GetHealthReportCount(), 3U);
        }

        TEST(ApplicationClientTest, GetInstanceId)
        {
            ApplicationClient _client(cSpecifier);
            EXPECT_EQ(_client.GetInstanceId(), "TestApp");
        }

        TEST(ApplicationClientTest, RecoveryActionHandlerInvokedOnRequest)
        {
            ApplicationClient _client(cSpecifier);
            ApplicationRecoveryAction _captured{ApplicationRecoveryAction::kNone};
            int _callCount{0};

            _client.SetRecoveryActionHandler(
                [&](ApplicationRecoveryAction action)
                {
                    _captured = action;
                    ++_callCount;
                });

            _client.RequestRecoveryAction(ApplicationRecoveryAction::kRestart);
            EXPECT_EQ(_captured, ApplicationRecoveryAction::kRestart);
            EXPECT_EQ(_callCount, 1);
        }

        TEST(ApplicationClientTest, HandlerNotCalledForNoneAction)
        {
            ApplicationClient _client(cSpecifier);
            int _callCount{0};

            _client.SetRecoveryActionHandler(
                [&](ApplicationRecoveryAction) { ++_callCount; });

            _client.RequestRecoveryAction(ApplicationRecoveryAction::kNone);
            EXPECT_EQ(_callCount, 0);
        }
    }
}
