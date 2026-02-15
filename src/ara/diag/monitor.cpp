/// @file src/ara/diag/monitor.cpp
/// @brief Implementation for monitor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./monitor.h"
#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor) : mSpecifier{specifier},
                                                                  mInitMonitor{initMonitor},
                                                                  mOffered{false},
                                                                  mDebouncer{nullptr},
                                                                  mEvent{nullptr}
        {
        }

        void Monitor::onEventStatusChanged(bool passed)
        {
            const int8_t cFailedFdc{127};
            const int8_t cPassedFdc{-128};
            const bool cTestNotCompleted{false};

            if (mEvent)
            {
                const auto cFdcResult{
                    mEvent->SetFaultDetectionCounter(
                        passed ? cPassedFdc : cFailedFdc)};
                if (!cFdcResult.HasValue())
                {
                    return;
                }

                (void)mEvent->SetEventStatusBits({
                    {EventStatusBit::kTestFailed, !passed},
                    {EventStatusBit::kTestNotCompletedThisOperationCycle, cTestNotCompleted}});
            }
        }

        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor,
            CounterBased defaultValues) : Monitor(specifier, initMonitor)
        {
            auto _callback{
                std::bind(
                    &Monitor::onEventStatusChanged, this, std::placeholders::_1)};

            mDebouncer =
                new debouncing::CounterBasedDebouncer(_callback, defaultValues);
        }

        Monitor::Monitor(
            const core::InstanceSpecifier &specifier,
            std::function<void(InitMonitorReason)> initMonitor,
            TimeBased defaultValues) : Monitor(specifier, initMonitor)
        {
            auto _callback{
                std::bind(
                    &Monitor::onEventStatusChanged, this, std::placeholders::_1)};

            mDebouncer =
                new debouncing::TimerBasedDebouncer(_callback, defaultValues);
        }

        void Monitor::ReportMonitorAction(MonitorAction action)
        {
            if (mOffered)
            {
                switch (action)
                {
                case MonitorAction::kPassed:
                    mDebouncer->ReportPassed();
                    break;

                case MonitorAction::kFailed:
                    mDebouncer->ReportFailed();
                    break;

                case MonitorAction::kPrepassed:
                    mDebouncer->ReportPrepassed();
                    break;

                case MonitorAction::kPrefailed:
                    mDebouncer->ReportPrefailed();
                    break;

                case MonitorAction::kFreezeDebouncing:
                    mDebouncer->Freeze();
                    break;

                case MonitorAction::kResetDebouncing:
                    mDebouncer->Reset();
                    break;

                case MonitorAction::kResetTestFailed:
                    if (mEvent)
                    {
                        (void)mEvent->SetEventStatusBits({
                            {EventStatusBit::kTestFailed, false}});
                    }
                    break;

                case MonitorAction::kFdcThresholdReached:
                    if (mEvent)
                    {
                        const int8_t cFailedFdc{127};
                        const auto cFdcResult{
                            mEvent->SetFaultDetectionCounter(cFailedFdc)};
                        if (!cFdcResult.HasValue())
                        {
                            break;
                        }

                        (void)mEvent->SetEventStatusBits({
                            {EventStatusBit::kTestFailed, true},
                            {EventStatusBit::kTestNotCompletedThisOperationCycle, false}});
                    }
                    break;

                default:
                    throw std::invalid_argument("Reported monitor action is not supported.");
                }
            }
        }

        core::Result<void> Monitor::AttachEvent(Event *event)
        {
            if (event == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kInvalidArgument));
            }

            mEvent = event;
            return core::Result<void>{};
        }

        core::Result<void> Monitor::Offer()
        {
            if (mOffered)
            {
                core::ErrorCode _errorCode{
                    MakeErrorCode(DiagErrc::kAlreadyOffered)};
                auto _result{core::Result<void>::FromError(_errorCode)};

                return _result;
            }
            else
            {
                mOffered = true;
                core::Result<void> _result;
                if (mInitMonitor)
                {
                    mInitMonitor(InitMonitorReason::kReenabled);
                }

                return _result;
            }
        }

        void Monitor::StopOffer()
        {
            if (mOffered)
            {
                mOffered = false;

                if (mInitMonitor)
                {
                    mInitMonitor(InitMonitorReason::kDisabled);
                }
            }
        }

        bool Monitor::IsOffered() const noexcept
        {
            return mOffered;
        }

        bool Monitor::HasAttachedEvent() const noexcept
        {
            return mEvent != nullptr;
        }

        Monitor::~Monitor() noexcept
        {
            delete mDebouncer;
        }
    }
}
