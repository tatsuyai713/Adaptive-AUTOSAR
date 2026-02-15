#include <gtest/gtest.h>
#include "../../../src/ara/diag/monitor.h"
#include "../../../src/ara/diag/diag_error_domain.h"

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

            auto _attachResult{
                _monitor.AttachEvent(&_event)};
            ASSERT_TRUE(_attachResult.HasValue());
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
            ASSERT_TRUE(_monitor.AttachEvent(&_event).HasValue());
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kFdcThresholdReached);

            const auto _fdcResult = _event.GetFaultDetectionCounter();
            ASSERT_TRUE(_fdcResult.HasValue());
            EXPECT_EQ(_fdcResult.Value(), 127);
        }

        TEST(MonitorTest, AttachEventRejectsNullPointer)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason)
            {
            };
            CounterBased _defaultValues;

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            const auto _attachResult{_monitor.AttachEvent(nullptr)};
            EXPECT_FALSE(_attachResult.HasValue());
            EXPECT_EQ(
                DiagErrc::kInvalidArgument,
                static_cast<DiagErrc>(_attachResult.Error().Value()));
        }

        TEST(MonitorTest, GetCurrentStatusInitiallyPending)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            EXPECT_EQ(
                debouncing::EventStatus::kPending,
                _monitor.GetCurrentStatus());
        }

        TEST(MonitorTest, GetCurrentStatusAfterPassedReport)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kPassed);
            EXPECT_EQ(
                debouncing::EventStatus::kPassed,
                _monitor.GetCurrentStatus());
        }

        TEST(MonitorTest, GetCurrentStatusAfterFailedReport)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(
                debouncing::EventStatus::kFailed,
                _monitor.GetCurrentStatus());
        }

        TEST(MonitorTest, GetFaultDetectionCounterInitiallyZero)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            EXPECT_EQ(0, _monitor.GetFaultDetectionCounter());
        }

        TEST(MonitorTest, GetFaultDetectionCounterAfterFailed)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{
                127, -128, 1, 1, 0, 0, false, false};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(127, _monitor.GetFaultDetectionCounter());
        }

        TEST(MonitorTest, GetFaultDetectionCounterAfterPassed)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{
                127, -128, 1, 1, 0, 0, false, false};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kPassed);
            EXPECT_EQ(-128, _monitor.GetFaultDetectionCounter());
        }

        TEST(MonitorTest, ResetDebouncingResetsStatus)
        {
            core::InstanceSpecifier _specifier("Instance0");
            auto _initMonitor = [](InitMonitorReason) {};
            CounterBased _defaultValues{};

            Monitor _monitor(_specifier, _initMonitor, _defaultValues);
            ASSERT_TRUE(_monitor.Offer().HasValue());

            _monitor.ReportMonitorAction(MonitorAction::kFailed);
            EXPECT_EQ(
                debouncing::EventStatus::kFailed,
                _monitor.GetCurrentStatus());

            _monitor.ReportMonitorAction(MonitorAction::kResetDebouncing);
            EXPECT_EQ(
                debouncing::EventStatus::kPending,
                _monitor.GetCurrentStatus());
            EXPECT_EQ(0, _monitor.GetFaultDetectionCounter());
        }
    }
}
