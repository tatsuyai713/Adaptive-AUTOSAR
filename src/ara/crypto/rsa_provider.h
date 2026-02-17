/// @file src/ara/crypto/rsa_provider.h
/// @brief RSA asymmetric cryptography provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef RSA_PROVIDER_H
#define RSA_PROVIDER_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief RSA key pair in DER encoding.
        struct RsaKeyPair
        {
            std::vector<std::uint8_t> PublicKeyDer;
            std::vector<std::uint8_t> PrivateKeyDer;
        };

        /// @brief Generate an RSA key pair.
        /// @param keySizeBits Key size in bits (2048 or 4096).
        /// @returns RSA key pair in DER format, or a crypto domain error.
        core::Result<RsaKeyPair> GenerateRsaKeyPair(
            std::uint32_t keySizeBits = 2048U);

        /// @brief Sign data with RSA private key (PKCS#1 v1.5).
        /// @param data Input data to sign.
        /// @param privateKeyDer RSA private key in DER format.
        /// @param algorithm Digest algorithm for signing.
        /// @returns Signature bytes or a crypto domain error.
        core::Result<std::vector<std::uint8_t>> RsaSign(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &privateKeyDer,
            DigestAlgorithm algorithm = DigestAlgorithm::kSha256);

        /// @brief Verify an RSA signature (PKCS#1 v1.5).
        /// @param data Original data.
        /// @param signature Signature to verify.
        /// @param publicKeyDer RSA public key in DER format.
        /// @param algorithm Digest algorithm used for signing.
        /// @returns true if valid, false if invalid, or a crypto domain error.
        core::Result<bool> RsaVerify(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &signature,
            const std::vector<std::uint8_t> &publicKeyDer,
            DigestAlgorithm algorithm = DigestAlgorithm::kSha256);

        /// @brief Encrypt data with RSA public key (OAEP padding).
        /// @param plaintext Data to encrypt (must be shorter than key size minus padding).
        /// @param publicKeyDer RSA public key in DER format.
        /// @returns Ciphertext bytes or a crypto domain error.
        core::Result<std::vector<std::uint8_t>> RsaEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &publicKeyDer);

        /// @brief Decrypt data with RSA private key (OAEP padding).
        /// @param ciphertext Encrypted data.
        /// @param privateKeyDer RSA private key in DER format.
        /// @returns Plaintext bytes or a crypto domain error.
        core::Result<std::vector<std::uint8_t>> RsaDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &privateKeyDer);
    }
}

#endif
