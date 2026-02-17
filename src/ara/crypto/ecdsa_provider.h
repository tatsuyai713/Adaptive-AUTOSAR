/// @file src/ara/crypto/ecdsa_provider.h
/// @brief ECDSA asymmetric cryptography provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ECDSA_PROVIDER_H
#define ECDSA_PROVIDER_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Supported elliptic curves.
        enum class EllipticCurve : std::uint32_t
        {
            kP256 = 0,   ///< NIST P-256 (secp256r1)
            kP384 = 1    ///< NIST P-384 (secp384r1)
        };

        /// @brief Elliptic curve key pair in DER encoding.
        struct EcKeyPair
        {
            std::vector<std::uint8_t> PublicKeyDer;
            std::vector<std::uint8_t> PrivateKeyDer;
        };

        /// @brief Generate an EC key pair for the specified curve.
        /// @param curve Elliptic curve to use.
        /// @returns EC key pair in DER format, or a crypto domain error.
        core::Result<EcKeyPair> GenerateEcKeyPair(
            EllipticCurve curve = EllipticCurve::kP256);

        /// @brief Sign data with ECDSA.
        /// @param data Input data to sign.
        /// @param privateKeyDer EC private key in DER format.
        /// @param algorithm Digest algorithm for signing.
        /// @returns Signature bytes or a crypto domain error.
        core::Result<std::vector<std::uint8_t>> EcdsaSign(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &privateKeyDer,
            DigestAlgorithm algorithm = DigestAlgorithm::kSha256);

        /// @brief Verify an ECDSA signature.
        /// @param data Original data.
        /// @param signature Signature to verify.
        /// @param publicKeyDer EC public key in DER format.
        /// @param algorithm Digest algorithm used for signing.
        /// @returns true if valid, false if invalid, or a crypto domain error.
        core::Result<bool> EcdsaVerify(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &signature,
            const std::vector<std::uint8_t> &publicKeyDer,
            DigestAlgorithm algorithm = DigestAlgorithm::kSha256);
    }
}

#endif
