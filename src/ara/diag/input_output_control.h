/// @file src/ara/diag/input_output_control.h
/// @brief ara::diag::InputOutputControl — UDS 0x2F handler.
/// @details Implements AUTOSAR AP diagnostic InputOutputControlByIdentifier
///          service (ISO 14229-1 §10.6, UDS SID 0x2F).
///
///          InputOutputControlByIdentifier allows external test equipment to
///          override (or release) ECU I/O signals identified by a 16-bit DID.
///          Typical uses include:
///          - Forcing actuator states during end-of-line tests
///          - Reading current I/O states for diagnostic verification
///          - Freeze-frame control during active DTC evaluation
///
///          Control options (sub-function via controlOptionRecord[0]):
///          - 0x00 returnControlToEcu  — release override, resume normal ECU control
///          - 0x01 resetToDefault      — reset I/O to its programmed default
///          - 0x02 freezeCurrentState  — hold current output value
///          - 0x03 shortTermAdjustment — apply a specific output value
///
///          Usage:
///          @code
///          InputOutputControl ioCtrl(spec);
///
///          // Register actuator override handler for DID 0x1234 (throttle override)
///          ioCtrl.RegisterControlHandler(0x1234,
///              [](InputOutputControl::ControlOption opt,
///                 const std::vector<uint8_t> &data) -> bool {
///                  if (opt == InputOutputControl::ControlOption::kShortTermAdjustment) {
///                      float throttle = data[0] / 255.0f * 100.0f;
///                      actuator.SetThrottle(throttle);
///                  } else if (opt == InputOutputControl::ControlOption::kReturnControlToEcu) {
///                      actuator.ReleaseOverride();
///                  }
///                  return true;
///              });
///
///          // Register read handler (for data record in response)
///          ioCtrl.RegisterReadHandler(0x1234, []() -> std::vector<uint8_t> {
///              return { static_cast<uint8_t>(actuator.GetCurrentValue()) };
///          });
///
///          ioCtrl.Offer();
///          @endcode
///
///          Reference: ISO 14229-1 §10.6, AUTOSAR SWS_Diag §7.6.2.7
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_INPUT_OUTPUT_CONTROL_H
#define ARA_DIAG_INPUT_OUTPUT_CONTROL_H

#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <vector>
#include "../core/instance_specifier.h"
#include "./routing/routable_uds_service.h"
#include "./reentrancy.h"

namespace ara
{
    namespace diag
    {
        /// @brief UDS 0x2F InputOutputControlByIdentifier handler.
        class InputOutputControl : public routing::RoutableUdsService
        {
        public:
            /// @brief Control option codes (controlOptionRecord byte[0]).
            enum class ControlOption : uint8_t
            {
                kReturnControlToEcu  = 0x00, ///< Release override; resume ECU control
                kResetToDefault      = 0x01, ///< Reset output to default value
                kFreezeCurrentState  = 0x02, ///< Freeze/hold current output
                kShortTermAdjustment = 0x03, ///< Apply specified value (data follows)
            };

            /// @brief Callback to apply an I/O control command for a specific DID.
            /// @param option Control option requested by the tester.
            /// @param data   For kShortTermAdjustment: the override value bytes.
            ///               Empty for other options.
            /// @returns True if command was accepted; false → NRC 0x31.
            using ControlHandler =
                std::function<bool(ControlOption option,
                                   const std::vector<uint8_t> &data)>;

            /// @brief Optional callback to read the current signal value for the
            ///        controlStatusRecord included in the positive response.
            using ReadHandler = std::function<std::vector<uint8_t>()>;

            static constexpr uint8_t cSid{0x2F};
            static constexpr uint8_t cNrcRequestOutOfRange{0x31};
            static constexpr uint8_t cNrcConditionsNotCorrect{0x22};

            explicit InputOutputControl(
                const ara::core::InstanceSpecifier &specifier,
                ReentrancyType reentrancyType = ReentrancyType::kNot);

            ~InputOutputControl() noexcept override = default;

            InputOutputControl(const InputOutputControl &) = delete;
            InputOutputControl &operator=(const InputOutputControl &) = delete;

            /// @brief Register a control handler for a DID.
            void RegisterControlHandler(uint16_t did, ControlHandler handler);

            /// @brief Register a read-back handler for the response status record.
            void RegisterReadHandler(uint16_t did, ReadHandler handler);

            /// @brief Unregister both handlers for a DID.
            void UnregisterHandlers(uint16_t did);

            std::future<OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override;

        private:
            mutable std::mutex mMutex;
            std::map<uint16_t, ControlHandler> mControlHandlers;
            std::map<uint16_t, ReadHandler> mReadHandlers;
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_INPUT_OUTPUT_CONTROL_H
