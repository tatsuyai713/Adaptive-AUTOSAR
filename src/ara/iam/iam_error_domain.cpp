/// @file src/ara/iam/iam_error_domain.cpp
/// @brief Implementation for iam error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        IamErrorDomain::IamErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *IamErrorDomain::Name() const noexcept
        {
            return "Iam";
        }

        const char *IamErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            IamErrc _code{static_cast<IamErrc>(errorCode)};
            switch (_code)
            {
            case IamErrc::kInvalidArgument:
                return "Invalid IAM argument.";
            case IamErrc::kPolicyStoreError:
                return "IAM policy store failure.";
            case IamErrc::kPolicyFileParseError:
                return "Policy file format is invalid.";
            default:
                return "Unknown IAM error.";
            }
        }

        core::ErrorCode MakeErrorCode(IamErrc code) noexcept
        {
            static const IamErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
