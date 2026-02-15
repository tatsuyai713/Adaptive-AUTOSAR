#include <gtest/gtest.h>
#include "../../../src/ara/diag/monitor.h"

namespace ara
{
    namespace diag
    {
        TEST(MonitorTest, CounterBasedOfferScenario)
        {
            core::InstanceSpecifier _specifier("Instance0");
            InitMonitorReason _currentReason{InitMonitorReason::kClear};
            auto _initMonitor = [&](InitMonitorReason newReason)
            {
                _currentReason = newReason;
            };
            CounterBased _defaultValues;

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);

            core::Result<void> _result{_monitor.Offer()};
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(InitMonitorReason::kReenabled, _currentReason);

            core::Result<void> _newResult{_monitor.Offer()};
            EXPECT_FALSE(_newResult.HasValue());

            _monitor.StopOffer();
            EXPECT_EQ(InitMonitorReason::kDisabled, _currentReason);
        }

        TEST(MonitorTest, TimerBasedOfferScenario)
        {
            core::InstanceSpecifier _specifier("Instance0");
            InitMonitorReason _currentReason{InitMonitorReason::kClear};
            auto _initMonitor = [&](InitMonitorReason newReason)
            {
                _currentReason = newReason;
            };
            TimeBased _defaultValues;

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);

            core::Result<void> _result{_monitor.Offer()};
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(InitMonitorReason::kReenabled, _currentReason);

            core::Result<void> _newResult{_monitor.Offer()};
            EXPECT_FALSE(_newResult.HasValue());

            _monitor.StopOffer();
            EXPECT_EQ(InitMonitorReason::kDisabled, _currentReason);
        }

        TEST(MonitorTest, OfferStateAndAttachmentQueries)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason)
            {
            };
            CounterBased _defaultValues;
            Event _event(_specifier);

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            EXPECT_FALSE(_monitor.IsOffered());
            EXPECT_FALSE(_monitor.HasAttachedEvent());

            _monitor.AttachEvent(&_event);
            EXPECT_TRUE(_monitor.HasAttachedEvent());

            auto _offerResult = _monitor.Offer();
            EXPECT_TRUE(_offerResult.HasValue());
            EXPECT_TRUE(_monitor.IsOffered());

            _monitor.StopOffer();
            EXPECT_FALSE(_monitor.IsOffered());
        }

        TEST(MonitorTest, FdcThresholdReachedUpdatesAttachedEvent)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason)
            {
            };
            CounterBased _defaultValues;
            Event _event(_specifier);

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            _monitor.AttachEvent(&_event);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kFdcThresholdReached);

            const auto _fdcResult = _event.GetFaultDetectionCounter();
            ASSERT_TRUE(_fdcResult.HasValue());
            EXPECT_EQ(_fdcResult.Value(), 127);
        }
    }
}
