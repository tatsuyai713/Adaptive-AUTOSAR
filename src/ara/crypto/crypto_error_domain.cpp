/// @file src/ara/crypto/crypto_error_domain.cpp
/// @brief Implementation for crypto error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        CryptoErrorDomain::CryptoErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *CryptoErrorDomain::Name() const noexcept
        {
            return "Crypto";
        }

        const char *CryptoErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            CryptoErrc _code{static_cast<CryptoErrc>(errorCode)};
            switch (_code)
            {
            case CryptoErrc::kInvalidArgument:
                return "Invalid cryptographic argument.";
            case CryptoErrc::kUnsupportedAlgorithm:
                return "Unsupported cryptographic algorithm.";
            case CryptoErrc::kEntropySourceFailure:
                return "Entropy source is not available.";
            case CryptoErrc::kCryptoProviderFailure:
                return "Crypto provider failure.";
            case CryptoErrc::kInvalidKeySize:
                return "Invalid symmetric key length.";
            case CryptoErrc::kEncryptionFailure:
                return "Encryption operation failed.";
            case CryptoErrc::kDecryptionFailure:
                return "Decryption operation or padding error.";
            case CryptoErrc::kSignatureFailure:
                return "Digital signature operation failed.";
            case CryptoErrc::kVerificationFailure:
                return "Signature or certificate verification failed.";
            case CryptoErrc::kKeyGenerationFailure:
                return "Asymmetric key generation failed.";
            case CryptoErrc::kInvalidKeyFormat:
                return "Key material is malformed or unsupported.";
            case CryptoErrc::kCertificateParseError:
                return "X.509 certificate parsing failed.";
            case CryptoErrc::kCertificateVerifyError:
                return "X.509 certificate chain verification failed.";
            case CryptoErrc::kSlotNotFound:
                return "Requested key slot does not exist.";
            case CryptoErrc::kSlotAlreadyExists:
                return "Key slot with the given ID already exists.";
            default:
                return "Unknown cryptographic error.";
            }
        }

        core::ErrorCode MakeErrorCode(CryptoErrc code) noexcept
        {
            static const CryptoErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
