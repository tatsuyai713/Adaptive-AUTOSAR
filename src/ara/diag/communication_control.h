/// @file src/ara/diag/communication_control.h
/// @brief ara::diag::CommunicationControl — UDS 0x28 handler.
/// @details Implements AUTOSAR AP diagnostic CommunicationControl service
///          (ISO 14229-1 §10.7, UDS SID 0x28).
///
///          CommunicationControl enables/disables Tx and/or Rx on one or more
///          communication types (normal/NM/network management). Tester tools
///          call this service during reprogramming sessions to suppress normal
///          communication and avoid interference.
///
///          Supported sub-functions:
///          - 0x00 enableRxAndTx          — resume normal communication
///          - 0x01 enableRxAndDisableTx   — suppress Tx only
///          - 0x02 disableRxAndEnableTx   — suppress Rx only
///          - 0x03 disableRxAndTx         — suppress all communication
///
///          Communication types (byte 2):
///          - 0x01 normalCommunicationMessages
///          - 0x02 nmCommunicationMessages
///          - 0x03 networkManagementCommunicationMessages (combined)
///
///          Usage:
///          @code
///          CommunicationControl comCtrl(spec);
///          comCtrl.SetControlCallback(
///              [](CommunicationControl::SubFunction sf,
///                 CommunicationControl::CommType ct) {
///                  if (sf == CommunicationControl::SubFunction::kDisableRxAndTx) {
///                      // suspend CAN Tx/Rx
///                  }
///              });
///          comCtrl.Offer();
///          @endcode
///
///          Reference: ISO 14229-1 §10.7, AUTOSAR SWS_Diag §7.6.2.8
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_COMMUNICATION_CONTROL_H
#define ARA_DIAG_COMMUNICATION_CONTROL_H

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
        /// @brief UDS 0x28 CommunicationControl handler.
        class CommunicationControl : public routing::RoutableUdsService
        {
        public:
            /// @brief UDS 0x28 sub-function codes.
            enum class SubFunction : uint8_t
            {
                kEnableRxAndTx        = 0x00, ///< Resume full communication
                kEnableRxAndDisableTx = 0x01, ///< Receive-only mode
                kDisableRxAndEnableTx = 0x02, ///< Transmit-only mode
                kDisableRxAndTx       = 0x03, ///< Suppress all communication
            };

            /// @brief Communication type bitmask (byte 2 of request).
            enum class CommType : uint8_t
            {
                kNormal           = 0x01, ///< Normal application messages
                kNm               = 0x02, ///< Network management messages
                kNormalAndNm      = 0x03, ///< Both normal and NM messages
            };

            /// @brief Callback invoked when the tester changes communication state.
            /// @param subFunc  Requested sub-function (enable/disable mode).
            /// @param commType Communication type affected.
            using ControlCallback =
                std::function<void(SubFunction subFunc, CommType commType)>;

            /// @brief UDS Service ID
            static constexpr uint8_t cSid{0x28};
            static constexpr uint8_t cNrcSubFunctionNotSupported{0x12};
            static constexpr uint8_t cNrcRequestOutOfRange{0x31};

            explicit CommunicationControl(
                const ara::core::InstanceSpecifier &specifier,
                ReentrancyType reentrancyType = ReentrancyType::kNot);

            ~CommunicationControl() noexcept override = default;

            CommunicationControl(const CommunicationControl &) = delete;
            CommunicationControl &operator=(const CommunicationControl &) = delete;

            /// @brief Register callback for control requests.
            void SetControlCallback(ControlCallback callback);

            /// @brief Get the last applied sub-function.
            SubFunction GetCurrentSubFunction() const noexcept;

            /// @brief Get the last applied communication type.
            CommType GetCurrentCommType() const noexcept;

            std::future<OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override;

        private:
            mutable std::mutex mMutex;
            ControlCallback mCallback;
            SubFunction mCurrentSubFunc{SubFunction::kEnableRxAndTx};
            CommType mCurrentCommType{CommType::kNormalAndNm};
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_COMMUNICATION_CONTROL_H
