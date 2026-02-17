/// @file src/ara/crypto/crypto_error_domain.h
/// @brief Declarations for crypto error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CRYPTO_ERROR_DOMAIN_H
#define CRYPTO_ERROR_DOMAIN_H

#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Error codes for ara::crypto subset implementation.
        enum class CryptoErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidArgument = 1,
            kUnsupportedAlgorithm = 2,
            kEntropySourceFailure = 3,
            kCryptoProviderFailure = 4,
            kInvalidKeySize = 5,        ///< Invalid symmetric key length
            kEncryptionFailure = 6,     ///< Encryption operation failed
            kDecryptionFailure = 7,     ///< Decryption operation or padding error
            kSignatureFailure = 8,      ///< Digital signature operation failed
            kVerificationFailure = 9,   ///< Signature or certificate verification failed
            kKeyGenerationFailure = 10, ///< Asymmetric key generation failed
            kInvalidKeyFormat = 11,     ///< Key material is malformed or unsupported
            kCertificateParseError = 12,///< X.509 certificate parsing failed
            kCertificateVerifyError = 13,///< X.509 certificate chain verification failed
            kSlotNotFound = 14,         ///< Requested key slot does not exist
            kSlotAlreadyExists = 15     ///< Key slot with the given ID already exists
        };

        /// @brief Error domain for ara::crypto subset implementation.
        class CryptoErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000601};

        public:
            CryptoErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::crypto domain.
        /// @param code Crypto error code.
        /// @returns Error code bound to CryptoErrorDomain.
        core::ErrorCode MakeErrorCode(CryptoErrc code) noexcept;
    }
}

#endif
