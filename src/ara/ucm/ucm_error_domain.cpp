#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        UcmErrorDomain::UcmErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *UcmErrorDomain::Name() const noexcept
        {
            return "Ucm";
        }

        const char *UcmErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            UcmErrc _code{static_cast<UcmErrc>(errorCode)};

            switch (_code)
            {
            case UcmErrc::kInvalidArgument:
                return "Invalid argument.";
            case UcmErrc::kInvalidState:
                return "Operation is not allowed in current update state.";
            case UcmErrc::kNoActiveSession:
                return "No active update session.";
            case UcmErrc::kPackageNotStaged:
                return "No software package is staged.";
            case UcmErrc::kVerificationFailed:
                return "Software package verification failed.";
            case UcmErrc::kSessionCancelled:
                return "Software update session has been cancelled.";
            case UcmErrc::kDowngradeNotAllowed:
                return "Software downgrade is not allowed.";
            case UcmErrc::kClusterNotFound:
                return "Software cluster is not known.";
            default:
                return "Unknown update and configuration management error.";
            }
        }

        core::ErrorCode MakeErrorCode(UcmErrc code) noexcept
        {
            static const UcmErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
