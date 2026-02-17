/// @file src/ara/diag/control_dtc_setting.h
/// @brief ara::diag::ControlDtcSetting — UDS 0x85 handler.
/// @details Implements AUTOSAR AP diagnostic ControlDtcSetting service
///          (ISO 14229-1 §10.8, UDS SID 0x85).
///
///          ControlDtcSetting enables or disables the update of DTC status bits
///          (DTC setting). Tester tools call this during reprogramming sessions
///          to prevent spurious DTC entries from being recorded during flashing.
///
///          Sub-functions:
///          - 0x01 on  — re-enable DTC status bit updates (default)
///          - 0x02 off — freeze/suspend DTC status bit updates
///
///          Usage:
///          @code
///          ControlDtcSetting dtcCtrl(spec);
///          dtcCtrl.SetSettingCallback([](bool enabled) {
///              if (!enabled) {
///                  monitor.DisableMonitoring(); // stop recording DTCs
///              } else {
///                  monitor.EnableMonitoring();
///              }
///          });
///          dtcCtrl.Offer();
///          @endcode
///
///          Reference: ISO 14229-1 §10.8, AUTOSAR SWS_Diag §7.6.2.9
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_CONTROL_DTC_SETTING_H
#define ARA_DIAG_CONTROL_DTC_SETTING_H

#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include "../core/instance_specifier.h"
#include "./routing/routable_uds_service.h"
#include "./reentrancy.h"

namespace ara
{
    namespace diag
    {
        /// @brief UDS 0x85 ControlDtcSetting handler.
        class ControlDtcSetting : public routing::RoutableUdsService
        {
        public:
            /// @brief UDS 0x85 sub-function codes.
            enum class SettingType : uint8_t
            {
                kOn  = 0x01, ///< Enable DTC status bit updates
                kOff = 0x02, ///< Disable DTC status bit updates
            };

            /// @brief Callback invoked when DTC setting state changes.
            /// @param enabled True = DTC updates enabled (sub-func 0x01),
            ///                False = DTC updates disabled (sub-func 0x02).
            using SettingCallback = std::function<void(bool enabled)>;

            static constexpr uint8_t cSid{0x85};
            static constexpr uint8_t cNrcSubFunctionNotSupported{0x12};

            explicit ControlDtcSetting(
                const ara::core::InstanceSpecifier &specifier,
                ReentrancyType reentrancyType = ReentrancyType::kNot);

            ~ControlDtcSetting() noexcept override = default;

            ControlDtcSetting(const ControlDtcSetting &) = delete;
            ControlDtcSetting &operator=(const ControlDtcSetting &) = delete;

            /// @brief Register callback for DTC setting changes.
            void SetSettingCallback(SettingCallback callback);

            /// @brief Get current DTC setting state (true = enabled).
            bool IsDtcSettingEnabled() const noexcept;

            std::future<OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override;

        private:
            mutable std::mutex mMutex;
            SettingCallback mCallback;
            bool mDtcSettingEnabled{true};
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_CONTROL_DTC_SETTING_H
