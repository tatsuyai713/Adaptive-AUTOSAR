#include <gtest/gtest.h>
#include "../../../src/ara/phm/health_channel.h"

namespace ara
{
    namespace phm
    {
        TEST(HealthChannelTest, Constructor)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            EXPECT_EQ(HealthStatus::kOk, _channel.GetLastReportedStatus());
        }

        TEST(HealthChannelTest, ReportHealthStatus)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            core::Result<void> _result = _channel.ReportHealthStatus(HealthStatus::kFailed);
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(HealthStatus::kFailed, _channel.GetLastReportedStatus());
        }

        TEST(HealthChannelTest, ReportMultipleStatuses)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            _channel.ReportHealthStatus(HealthStatus::kFailed);
            EXPECT_EQ(HealthStatus::kFailed, _channel.GetLastReportedStatus());

            _channel.ReportHealthStatus(HealthStatus::kOk);
            EXPECT_EQ(HealthStatus::kOk, _channel.GetLastReportedStatus());

            _channel.ReportHealthStatus(HealthStatus::kExpired);
            EXPECT_EQ(HealthStatus::kExpired, _channel.GetLastReportedStatus());

            _channel.ReportHealthStatus(HealthStatus::kDeactivated);
            EXPECT_EQ(HealthStatus::kDeactivated, _channel.GetLastReportedStatus());
        }

        TEST(HealthChannelTest, MoveConstructor)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};
            _channel.ReportHealthStatus(HealthStatus::kFailed);

            HealthChannel _movedChannel{std::move(_channel)};
            EXPECT_EQ(HealthStatus::kFailed, _movedChannel.GetLastReportedStatus());
        }
    }
}
