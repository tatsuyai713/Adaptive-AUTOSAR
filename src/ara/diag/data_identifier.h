/// @file src/ara/diag/data_identifier.h
/// @brief ara::diag::DataIdentifierService — UDS 0x22/0x2E DID handler.
/// @details Implements AUTOSAR AP diagnostic Data Identifier services:
///          - UDS Service 0x22 (ReadDataByIdentifier): Read one or more 16-bit
///            Data Identifiers (DIDs) in a single request.
///          - UDS Service 0x2E (WriteDataByIdentifier): Write a 16-bit DID.
///
///          Usage (register handlers, then route UDS requests through it):
///          @code
///          ara::core::InstanceSpecifier spec{"myApp/DiagService"};
///          DataIdentifierService didService(spec);
///
///          // Register a read handler for DID 0xF190 (VIN)
///          didService.RegisterReadHandler(0xF190, []() {
///              std::string vin = "1HGBH41JXMN109186";
///              return std::vector<uint8_t>(vin.begin(), vin.end());
///          });
///
///          // Register a write handler for DID 0x0100 (calibration)
///          didService.RegisterWriteHandler(0x0100,
///              [](const std::vector<uint8_t> &data) -> bool {
///                  // store data...
///                  return true; // true = write accepted
///              });
///
///          didService.Offer();
///          // Then pass incoming 0x22/0x2E requests to HandleMessage()
///          @endcode
///
///          Reference: ISO 14229-1 §10.3 (0x22), §10.5 (0x2E)
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_DATA_IDENTIFIER_H
#define ARA_DIAG_DATA_IDENTIFIER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./routing/routable_uds_service.h"
#include "./reentrancy.h"

namespace ara
{
    namespace diag
    {
        /// @brief Callback type for DID read operations.
        /// @returns The DID data bytes on success; empty vector if unavailable.
        using DidReadHandler = std::function<std::vector<uint8_t>()>;

        /// @brief Callback type for DID write operations.
        /// @param data The bytes to write.
        /// @returns True if write was accepted, false if rejected (NRC 0x22).
        using DidWriteHandler = std::function<bool(const std::vector<uint8_t> &data)>;

        /// @brief UDS 0x22 (ReadDataByIdentifier) and 0x2E (WriteDataByIdentifier)
        ///        handler with per-DID read/write callback registry.
        ///
        /// @details Handles multi-DID reads (multiple DIDs in one 0x22 request)
        ///          and single-DID writes. Unsupported DIDs return NRC 0x31
        ///          (RequestOutOfRange). Write-protected DIDs (no write handler)
        ///          return NRC 0x31.
        class DataIdentifierService : public routing::RoutableUdsService
        {
        public:
            // UDS Service IDs
            static constexpr uint8_t cSidRead{0x22};  ///< ReadDataByIdentifier
            static constexpr uint8_t cSidWrite{0x2E}; ///< WriteDataByIdentifier

            // NRC values
            static constexpr uint8_t cNrcRequestOutOfRange{0x31};
            static constexpr uint8_t cNrcConditionsNotCorrect{0x22};
            static constexpr uint8_t cNrcRequestTooLong{0x14};

            /// @brief Construct a combined 0x22/0x2E DID service.
            /// @param specifier Owner instance specifier.
            /// @param reentrancyType Service reentrancy mode.
            /// @param writeServiceId  Which SID this instance handles:
            ///        pass cSidRead (0x22) for read-only, or cSidWrite (0x2E)
            ///        for write-only. Use two instances for both.
            explicit DataIdentifierService(
                const ara::core::InstanceSpecifier &specifier,
                ReentrancyType reentrancyType = ReentrancyType::kNot,
                uint8_t serviceId = cSidRead);

            ~DataIdentifierService() noexcept override = default;

            // Non-copyable
            DataIdentifierService(const DataIdentifierService &) = delete;
            DataIdentifierService &operator=(const DataIdentifierService &) = delete;

            /// @brief Register a read handler for a specific DID.
            /// @param did  16-bit Data Identifier.
            /// @param handler Callback returning the DID data bytes.
            void RegisterReadHandler(uint16_t did, DidReadHandler handler);

            /// @brief Register a write handler for a specific DID.
            /// @param did  16-bit Data Identifier.
            /// @param handler Callback receiving the write data; returns true on success.
            void RegisterWriteHandler(uint16_t did, DidWriteHandler handler);

            /// @brief Remove the read handler for a specific DID.
            void UnregisterReadHandler(uint16_t did);

            /// @brief Remove the write handler for a specific DID.
            void UnregisterWriteHandler(uint16_t did);

            /// @brief Check whether a DID has a registered read handler.
            bool HasReadHandler(uint16_t did) const;

            /// @brief Check whether a DID has a registered write handler.
            bool HasWriteHandler(uint16_t did) const;

            /// @brief Handle an incoming UDS request (0x22 or 0x2E).
            std::future<OperationOutput> HandleMessage(
                const std::vector<uint8_t> &requestData,
                MetaInfo &metaInfo,
                CancellationHandler &&cancellationHandler) override;

        private:
            mutable std::mutex mMutex;
            std::map<uint16_t, DidReadHandler> mReadHandlers;
            std::map<uint16_t, DidWriteHandler> mWriteHandlers;

            OperationOutput handleReadRequest(const std::vector<uint8_t> &request);
            OperationOutput handleWriteRequest(const std::vector<uint8_t> &request);
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_DATA_IDENTIFIER_H
