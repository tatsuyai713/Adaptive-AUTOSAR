/// @file src/ara/diag/dtc_information.cpp
/// @brief Implementation for dtc information.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./dtc_information.h"
#include "./diag_error_domain.h"
#include <utility>

namespace ara
{
    namespace diag
    {
        namespace
        {
            core::ErrorCode makeDiagError(DiagErrc code)
            {
                return MakeErrorCode(code);
            }
        }

        DTCInformation::DTCInformation(const core::InstanceSpecifier &specifier) : mSpecifier{specifier},
                                                                                   mControlDtcStatus{ControlDtcStatusType::kDTCSettingOff}
        {
        }

        core::Result<UdsDtcStatusByteType> DTCInformation::GetCurrentStatus(uint32_t dtc)
        {
            const std::lock_guard<std::mutex> _lock{mMutex};
            auto _itr{mStatuses.find(dtc)};
            if (_itr == mStatuses.end())
            {
                auto _result{core::Result<UdsDtcStatusByteType>::FromError(
                    makeDiagError(DiagErrc::kNoSuchDTC))};
                return _result;
            }

            core::Result<UdsDtcStatusByteType> _result{_itr->second};
            return _result;
        }

        core::Result<void> DTCInformation::SetCurrentStatus(
            uint32_t dtc, UdsDtcStatusBitType mask, UdsDtcStatusByteType status)
        {
            std::function<void(uint32_t, UdsDtcStatusByteType, UdsDtcStatusByteType)> _dtcStatusNotifier;
            std::function<void(uint32_t)> _entriesNotifier;
            UdsDtcStatusByteType _oldStatus{0};
            UdsDtcStatusByteType _newStatus{0};
            uint32_t _numberOfEntries{0};
            bool _notifyStatusChange{false};
            bool _notifyEntryCount{false};

            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                auto _iterator{mStatuses.find(dtc)};

                if (_iterator == mStatuses.end())
                {
                    // Add the DTC status
                    mStatuses[dtc] = status;
                    _entriesNotifier = mNumberOfStoredEntriesNotifier;
                    _numberOfEntries = static_cast<uint32_t>(mStatuses.size());
                    _notifyEntryCount = true;
                }
                else
                {
                    // Edit the DTC status
                    UdsDtcStatusByteType _currentStatus{_iterator->second};
                    auto _differenceByte{
                        static_cast<uint8_t>(
                            _currentStatus.encodedBits ^ status.encodedBits)};

                    auto _maskByte{static_cast<uint8_t>(mask)};
                    auto _maskedDifferenceByte{
                        static_cast<uint8_t>(_differenceByte & _maskByte)};

                    // Edit the status byte if there is any difference
                    if (_maskedDifferenceByte)
                    {
                        // Flip the current status bits
                        auto _newStatusBits{
                            static_cast<uint8_t>(
                                _currentStatus.encodedBits ^ _maskedDifferenceByte)};

                        _iterator->second.encodedBits = _newStatusBits;
                        _dtcStatusNotifier = mDtcStatusChangedNotifier;
                        _oldStatus = _currentStatus;
                        _newStatus = _iterator->second;
                        _notifyStatusChange = true;
                    }
                }
            }

            if (_notifyEntryCount && _entriesNotifier)
            {
                _entriesNotifier(_numberOfEntries);
            }

            if (_notifyStatusChange && _dtcStatusNotifier)
            {
                _dtcStatusNotifier(dtc, _oldStatus, _newStatus);
            }

            return core::Result<void>{};
        }

        core::Result<void> DTCInformation::SetDTCStatusChangedNotifier(
            std::function<void(uint32_t, UdsDtcStatusByteType, UdsDtcStatusByteType)> notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    makeDiagError(DiagErrc::kInvalidArgument));
            }

            const std::lock_guard<std::mutex> _lock{mMutex};
            mDtcStatusChangedNotifier = notifier;
            core::Result<void> _result;

            return _result;
        }

        core::Result<uint32_t> DTCInformation::GetNumberOfStoredEntries()
        {
            const std::lock_guard<std::mutex> _lock{mMutex};
            auto _size{static_cast<uint32_t>(mStatuses.size())};
            core::Result<uint32_t> _result{_size};

            return _result;
        }

        core::Result<std::vector<uint32_t>> DTCInformation::GetStoredDtcIds()
        {
            const std::lock_guard<std::mutex> _lock{mMutex};
            std::vector<uint32_t> _dtcs;
            _dtcs.reserve(mStatuses.size());
            for (const auto &_pair : mStatuses)
            {
                _dtcs.push_back(_pair.first);
            }

            return core::Result<std::vector<uint32_t>>{std::move(_dtcs)};
        }

        core::Result<void> DTCInformation::SetNumberOfStoredEntriesNotifier(
            std::function<void(uint32_t)> notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    makeDiagError(DiagErrc::kInvalidArgument));
            }

            const std::lock_guard<std::mutex> _lock{mMutex};
            mNumberOfStoredEntriesNotifier = notifier;
            core::Result<void> _result;

            return _result;
        }

        core::Result<void> DTCInformation::Clear(uint32_t dtc)
        {
            std::function<void(uint32_t)> _entriesNotifier;
            uint32_t _newNumberOfEntries{0};
            bool _removed{false};
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                auto _iterator{mStatuses.find(dtc)};
                if (_iterator == mStatuses.end())
                {
                    auto _result{core::Result<void>::FromError(
                        makeDiagError(DiagErrc::kWrongDtc))};
                    return _result;
                }

                mStatuses.erase(_iterator);
                _entriesNotifier = mNumberOfStoredEntriesNotifier;
                _newNumberOfEntries = static_cast<uint32_t>(mStatuses.size());
                _removed = true;
            }

            if (_removed && _entriesNotifier)
            {
                _entriesNotifier(_newNumberOfEntries);
            }

            core::Result<void> _result;
            return _result;
        }

        core::Result<void> DTCInformation::ClearAll()
        {
            std::function<void(uint32_t)> _entriesNotifier;
            bool _hadEntries{false};
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                _hadEntries = !mStatuses.empty();
                mStatuses.clear();
                _entriesNotifier = mNumberOfStoredEntriesNotifier;
            }

            if (_hadEntries && _entriesNotifier)
            {
                _entriesNotifier(0);
            }

            return core::Result<void>{};
        }

        core::Result<ControlDtcStatusType> DTCInformation::GetControlDTCStatus()
        {
            const std::lock_guard<std::mutex> _lock{mMutex};
            core::Result<ControlDtcStatusType> _result{mControlDtcStatus};
            return _result;
        }

        core::Result<void> DTCInformation::SetControlDtcStatusNotifier(
            std::function<void(ControlDtcStatusType)> notifier)
        {
            if (!notifier)
            {
                return core::Result<void>::FromError(
                    makeDiagError(DiagErrc::kInvalidArgument));
            }

            const std::lock_guard<std::mutex> _lock{mMutex};
            mControlDtcStatusNotifier = notifier;
            core::Result<void> _result;

            return _result;
        }

        core::Result<void> DTCInformation::EnableControlDtc()
        {
            std::function<void(ControlDtcStatusType)> _notifier;
            bool _statusChanged{false};
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                if (mControlDtcStatus != ControlDtcStatusType::kDTCSettingOn)
                {
                    mControlDtcStatus = ControlDtcStatusType::kDTCSettingOn;
                    _statusChanged = true;
                    _notifier = mControlDtcStatusNotifier;
                }
            }

            if (_statusChanged && _notifier)
            {
                _notifier(ControlDtcStatusType::kDTCSettingOn);
            }

            core::Result<void> _result;
            return _result;
        }

        core::Result<void> DTCInformation::DisableControlDtc()
        {
            std::function<void(ControlDtcStatusType)> _notifier;
            bool _statusChanged{false};
            {
                const std::lock_guard<std::mutex> _lock{mMutex};
                if (mControlDtcStatus != ControlDtcStatusType::kDTCSettingOff)
                {
                    mControlDtcStatus = ControlDtcStatusType::kDTCSettingOff;
                    _statusChanged = true;
                    _notifier = mControlDtcStatusNotifier;
                }
            }

            if (_statusChanged && _notifier)
            {
                _notifier(ControlDtcStatusType::kDTCSettingOff);
            }

            return core::Result<void>{};
        }
    }
}
