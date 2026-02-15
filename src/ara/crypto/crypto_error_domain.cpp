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
