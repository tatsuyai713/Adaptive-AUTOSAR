#include <gtest/gtest.h>
#include "../../../src/ara/phm/logical_supervision.h"

namespace ara
{
    namespace phm
    {
        TEST(LogicalSupervisionTest, ValidCheckpointSequenceStaysOk)
        {
            LogicalSupervisionConfig _config;
            _config.initialCheckpoint = 1U;
            _config.transitions = {{1U, 2U}, {2U, 3U}, {3U, 1U}};

            LogicalSupervision _supervision{_config};
            _supervision.Start();
            _supervision.ReportCheckpoint(1U);
            _supervision.ReportCheckpoint(2U);
            _supervision.ReportCheckpoint(3U);

            EXPECT_EQ(LogicalSupervisionStatus::kOk, _supervision.GetStatus());
        }

        TEST(LogicalSupervisionTest, InvalidCheckpointSequenceExpiresAfterThreshold)
        {
            LogicalSupervisionConfig _config;
            _config.initialCheckpoint = 1U;
            _config.transitions = {{1U, 2U}};
            _config.failedThreshold = 2U;

            LogicalSupervision _supervision{_config};
            _supervision.Start();
            _supervision.ReportCheckpoint(3U);
            EXPECT_EQ(LogicalSupervisionStatus::kFailed, _supervision.GetStatus());
            _supervision.ReportCheckpoint(4U);
            EXPECT_EQ(LogicalSupervisionStatus::kExpired, _supervision.GetStatus());
        }

        TEST(LogicalSupervisionTest, CallbackCanQueryStatus)
        {
            LogicalSupervisionConfig _config;
            _config.initialCheckpoint = 1U;
            _config.transitions = {{1U, 2U}};

            LogicalSupervision _supervision{_config};
            bool _called{false};
            _supervision.SetStatusCallback(
                [&_supervision, &_called](LogicalSupervisionStatus status) {
                    _called = true;
                    EXPECT_EQ(status, _supervision.GetStatus());
                });

            _supervision.Start();
            _supervision.ReportCheckpoint(9U);

            EXPECT_TRUE(_called);
        }
    }
}
