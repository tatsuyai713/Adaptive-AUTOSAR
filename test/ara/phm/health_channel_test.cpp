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
            EXPECT_FALSE(_channel.IsOffered());
        }

        TEST(HealthChannelTest, OfferAndStopOffer)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            EXPECT_FALSE(_channel.IsOffered());

            auto _result = _channel.Offer();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_TRUE(_channel.IsOffered());

            _channel.StopOffer();
            EXPECT_FALSE(_channel.IsOffered());
        }

        TEST(HealthChannelTest, DoubleOfferFails)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            ASSERT_TRUE(_channel.Offer().HasValue());

            auto _result = _channel.Offer();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(HealthChannelTest, ReportHealthStatusRequiresOffer)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};

            auto _result = _channel.ReportHealthStatus(HealthStatus::kFailed);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(HealthChannelTest, ReportHealthStatus)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};
            ASSERT_TRUE(_channel.Offer().HasValue());

            core::Result<void> _result = _channel.ReportHealthStatus(HealthStatus::kFailed);
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(HealthStatus::kFailed, _channel.GetLastReportedStatus());
        }

        TEST(HealthChannelTest, ReportMultipleStatuses)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};
            ASSERT_TRUE(_channel.Offer().HasValue());

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
            ASSERT_TRUE(_channel.Offer().HasValue());
            _channel.ReportHealthStatus(HealthStatus::kFailed);

            HealthChannel _movedChannel{std::move(_channel)};
            EXPECT_EQ(HealthStatus::kFailed, _movedChannel.GetLastReportedStatus());
            EXPECT_TRUE(_movedChannel.IsOffered());
        }

        TEST(HealthChannelTest, StopOfferPreventsReporting)
        {
            core::InstanceSpecifier _specifier("test/instance");
            HealthChannel _channel{_specifier};
            ASSERT_TRUE(_channel.Offer().HasValue());
            ASSERT_TRUE(_channel.ReportHealthStatus(HealthStatus::kOk).HasValue());

            _channel.StopOffer();
            auto _result = _channel.ReportHealthStatus(HealthStatus::kFailed);
            EXPECT_FALSE(_result.HasValue());
            // Last reported status should remain kOk
            EXPECT_EQ(HealthStatus::kOk, _channel.GetLastReportedStatus());
        }
    }
}
