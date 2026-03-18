/// @file src/ara/crypto/crypto_service_provider.h
/// @brief CryptoServiceProvider class (SWS_CRYPT_20100).
/// @details Object-oriented wrapper around the ara::crypto free functions,
///          providing the SWS CryptoProvider interface.

#ifndef ARA_CRYPTO_CRYPTO_SERVICE_PROVIDER_H
#define ARA_CRYPTO_CRYPTO_SERVICE_PROVIDER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./crypto_context.h"
#include "./crypto_error_domain.h"
#include "./crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Object-oriented CryptoProvider per SWS_CRYPT_20100.
        ///
        /// Wraps the existing free-function crypto primitives into a single
        /// service-provider object that applications obtain via
        /// `LoadCryptoProvider()`.
        class CryptoServiceProvider
        {
        public:
            /// @brief Construct a CryptoServiceProvider.
            /// @param specifier Instance specifier identifying this provider.
            explicit CryptoServiceProvider(
                const core::InstanceSpecifier &specifier);

            ~CryptoServiceProvider() = default;
            CryptoServiceProvider(const CryptoServiceProvider &) = default;
            CryptoServiceProvider &operator=(const CryptoServiceProvider &) = default;
            CryptoServiceProvider(CryptoServiceProvider &&) = default;
            CryptoServiceProvider &operator=(CryptoServiceProvider &&) = default;

            /// @brief Compute a message digest.
            core::Result<std::vector<std::uint8_t>> ComputeDigest(
                const std::vector<std::uint8_t> &data,
                DigestAlgorithm algorithm = DigestAlgorithm::kSha256) const;

            /// @brief Generate random bytes.
            core::Result<std::vector<std::uint8_t>> GenerateRandomBytes(
                std::size_t byteCount) const;

            /// @brief Compute HMAC.
            core::Result<std::vector<std::uint8_t>> ComputeHmac(
                const std::vector<std::uint8_t> &data,
                const std::vector<std::uint8_t> &key,
                DigestAlgorithm algorithm = DigestAlgorithm::kSha256) const;

            /// @brief Generate a symmetric key.
            core::Result<std::vector<std::uint8_t>> GenerateSymmetricKey(
                std::size_t keyLengthBytes) const;

            /// @brief AES-CBC encrypt.
            core::Result<std::vector<std::uint8_t>> AesEncrypt(
                const std::vector<std::uint8_t> &plaintext,
                const std::vector<std::uint8_t> &key,
                const std::vector<std::uint8_t> &iv) const;

            /// @brief AES-CBC decrypt.
            core::Result<std::vector<std::uint8_t>> AesDecrypt(
                const std::vector<std::uint8_t> &ciphertext,
                const std::vector<std::uint8_t> &key,
                const std::vector<std::uint8_t> &iv) const;

            /// @brief Get the provider's instance identifier.
            const std::string &GetInstanceId() const noexcept;

        private:
            std::string mInstanceId;
        };

        /// @brief Factory function to obtain a CryptoServiceProvider (SWS_CRYPT_20100).
        /// @param specifier Instance specifier for the provider configuration.
        /// @returns A new CryptoServiceProvider instance.
        core::Result<CryptoServiceProvider> LoadCryptoProvider(
            const core::InstanceSpecifier &specifier);
    }
}

#endif
