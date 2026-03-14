/// @file src/ara/crypto/ecdh_provider.h
/// @brief ECDH (Elliptic Curve Diffie-Hellman) key agreement per AUTOSAR AP SWS_Crypto.
/// @details Provides key agreement, key import/export in PEM and DER formats.

#ifndef ARA_CRYPTO_ECDH_PROVIDER_H
#define ARA_CRYPTO_ECDH_PROVIDER_H

#include <cstdint>
#include <vector>
#include <string>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./ecdsa_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Derive a shared secret using ECDH (SWS_CRYPT_24100).
        /// @param ownPrivateKeyDer Own EC private key in DER format.
        /// @param peerPublicKeyDer Peer's EC public key in DER format.
        /// @returns Shared secret bytes or a crypto domain error.
        core::Result<std::vector<std::uint8_t>> EcdhDeriveSecret(
            const std::vector<std::uint8_t> &ownPrivateKeyDer,
            const std::vector<std::uint8_t> &peerPublicKeyDer);

        /// @brief Export an EC public key from DER to PEM format (SWS_CRYPT_30100).
        /// @param publicKeyDer EC public key in DER format.
        /// @returns PEM-encoded string or error.
        core::Result<std::string> ExportPublicKeyPem(
            const std::vector<std::uint8_t> &publicKeyDer);

        /// @brief Import an EC public key from PEM to DER format (SWS_CRYPT_30101).
        /// @param pem PEM-encoded public key string.
        /// @returns DER-encoded public key bytes or error.
        core::Result<std::vector<std::uint8_t>> ImportPublicKeyPem(
            const std::string &pem);

    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_ECDH_PROVIDER_H
