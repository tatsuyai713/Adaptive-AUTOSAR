/// @file src/ara/diag/clear_diagnostic_information.h
/// @brief ara::diag::ClearDiagnosticInformation — UDS 0x14 handler.
/// @details Implements AUTOSAR AP diagnostic ClearDiagnosticInformation service
///          (ISO 14229-1 §10.2, UDS SID 0x14).
///
///          ClearDiagnosticInformation clears stored DTC information from the
///          server's memory. The tester specifies a group-of-DTC value (3 bytes):
///          - 0xFFFFFF — clear all DTCs (allGroupOfDTC)
///          - 0xFFFF00 — clear all emission-related DTCs
///          - 0x000000-0xFFFEFF — clear a specific DTC or DTC group
///
///          Usage:
///          @code
///          ClearDiagnosticInformation clearDiag(spec);
///          clearDiag.SetClearCallback(
///              [&dtcStorage](uint32_t groupOfDtc) -> bool {
///                  if (groupOfDtc == 0xFFFFFF) {
///                      dtcStorage.ClearAll();
///                      return true;
///                  }
///                  return dtcStorage.Clear(groupOfDtc);
///              });
///          clearDiag.Offer();
///          @endcode
///
///          Reference: ISO 14229-1 §10.2, AUTOSAR SWS_Diag §7.6.2.3
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_CLEAR_DIAGNOSTIC_INFORMATION_H
#define ARA_DIAG_CLEAR_DIAGNOSTIC_INFORMATION_H

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
        /// @brief UDS 0x14 ClearDiagnosticInformation handler.
        class ClearDiagnosticInformation : public routing::RoutableUdsService
        {
        public:
            /// @brief Callback invoked to perform the actual DTC clear.
            /// @param groupOfDtc 24-bit group-of-DTC identifier:
            ///        0xFFFFFF = clear all DTCs.
            /// @returns True if clear was performed, false if DTC group not found
            ///          (NRC 0x31 RequestOutOfRange).
            using ClearCallback = std::function<bool(uint32_t groupOfDtc)>;

            static constexpr uint8_t cSid{0x14};
            static constexpr uint8_t cNrcRequestOutOfRange{0x31};
            static constexpr uint32_t cAllGroupOfDtc{0xFFFFFFU};

            explicit ClearDiagnosticInformation(
                const ara::core::InstanceSpecifier &specifier,
                ReentrancyType reentrancyType = ReentrancyType::kNot);

            ~ClearDiagnosticInformation() noexcept override = default;

            ClearDiagnosticInformation(const ClearDiagnosticInformation &) = delete;
            ClearDiagnosticInformation &operator=(const ClearDiagnosticInformation &) = delete;

            /// @brief Register callback for the actual DTC clear operation.
            void SetClearCallback(ClearCallback callback);

            std::future<OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override;

        private:
            mutable std::mutex mMutex;
            ClearCallback mCallback;
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_CLEAR_DIAGNOSTIC_INFORMATION_H
