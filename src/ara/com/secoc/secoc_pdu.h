/// @file src/ara/com/secoc/secoc_pdu.h
/// @brief SecOC PDU — Secure PDU authenticator and verifier.
/// @details Implements AUTOSAR SecOC secured PDU processing:
///          - Sender: append truncated MAC + freshness to payload
///          - Receiver: verify MAC + freshness, reject replays
///
///          Secured PDU layout (appended after payload):
///          | Payload (N bytes) | Freshness (W bytes) | Truncated MAC (T bytes) |
///
///          MAC computation input:
///          | DataID (2 bytes) | Freshness (W bytes) | Payload (N bytes) |
///
///          The MAC algorithm uses HMAC-SHA-256 (from ara::crypto).
///
///          Reference: AUTOSAR_SWS_SecureOnboardCommunication §7.3
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SECOC_SECOC_PDU_H
#define ARA_COM_SECOC_SECOC_PDU_H

#include <cstdint>
#include <vector>
#include <functional>
#include "../../core/result.h"
#include "./secoc_error_domain.h"
#include "./freshness_manager.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            /// @brief Configuration for a SecOC-protected PDU.
            struct SecOcPduConfig
            {
                /// @brief Unique PDU / signal identifier (included in MAC computation).
                uint16_t dataId{0x0000};

                /// @brief Number of freshness bytes appended to the secured PDU.
                /// @details Full freshness value; may be truncated for transmission.
                ///          Receiver must know the full freshness to verify.
                uint8_t freshnessLength{4};

                /// @brief Number of truncated freshness bytes in the wire format.
                /// @details Must be <= freshnessLength.
                ///          Set equal to freshnessLength for full freshness transmission.
                uint8_t truncatedFreshnessLength{4};

                /// @brief Number of truncated MAC bytes appended to the secured PDU.
                /// @details Full HMAC-SHA-256 = 32 bytes; typically truncated to 3–8 bytes.
                uint8_t truncatedMacLength{8};

                /// @brief Freshness counter configuration.
                FreshnessConfig freshnessConfig{};
            };

            /// @brief MAC computation function type.
            /// @details Signature: (key, data) → MAC bytes (32 bytes for HMAC-SHA-256)
            using MacFunction = std::function<std::vector<uint8_t>(
                const std::vector<uint8_t> & /*key*/,
                const std::vector<uint8_t> & /*data*/)>;

            /// @brief SecOC secured PDU processor.
            /// @details Handles authentication on transmit and verification on receive.
            ///
            ///          Sender usage:
            ///          @code
            ///          SecOcPdu secoc(config, key, macFn, &freshnessManager);
            ///          auto result = secoc.Protect(payload);
            ///          if (result.HasValue()) send(result.Value());
            ///          @endcode
            ///
            ///          Receiver usage:
            ///          @code
            ///          auto result = secoc.Verify(received_bytes);
            ///          if (result.HasValue()) process(result.Value());  // authenticated payload
            ///          @endcode
            class SecOcPdu
            {
            public:
                /// @brief Construct a SecOC PDU processor.
                /// @param config PDU configuration.
                /// @param key Symmetric key bytes for HMAC computation.
                /// @param macFn MAC computation function (e.g., HMAC-SHA-256).
                /// @param freshnessManager Freshness manager (shared ownership allowed).
                SecOcPdu(const SecOcPduConfig &config,
                         std::vector<uint8_t> key,
                         MacFunction macFn,
                         FreshnessManager *freshnessManager);

                ~SecOcPdu() = default;

                /// @brief Protect (authenticate) a payload for transmission.
                /// @param payload Unsecured payload bytes.
                /// @returns Secured PDU bytes (payload + freshness + truncated MAC),
                ///          or error if MAC computation or freshness retrieval failed.
                /// @post Freshness counter is incremented on success.
                ara::core::Result<std::vector<uint8_t>> Protect(
                    const std::vector<uint8_t> &payload);

                /// @brief Verify a received secured PDU.
                /// @param securedPdu Received bytes (payload + freshness + MAC).
                /// @returns Authenticated payload bytes (stripped of SecOC header),
                ///          or error if verification failed.
                /// @post Freshness counter is updated on success.
                ara::core::Result<std::vector<uint8_t>> Verify(
                    const std::vector<uint8_t> &securedPdu);

            private:
                SecOcPduConfig mConfig;
                std::vector<uint8_t> mKey;
                MacFunction mMacFn;
                FreshnessManager *mFreshnessManager;

                /// @brief Build MAC input: DataID || Freshness || Payload
                std::vector<uint8_t> buildMacInput(
                    const FreshnessValue &freshness,
                    const std::vector<uint8_t> &payload) const;
            };

        } // namespace secoc
    }     // namespace com
} // namespace ara

#endif // ARA_COM_SECOC_SECOC_PDU_H
