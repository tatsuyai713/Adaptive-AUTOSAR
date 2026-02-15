#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        ComErrorDomain::ComErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *ComErrorDomain::Name() const noexcept
        {
            return "Com";
        }

        const char *ComErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            ComErrc _code{static_cast<ComErrc>(errorCode)};

            switch (_code)
            {
            case ComErrc::kServiceNotAvailable:
                return "Service is not available.";
            case ComErrc::kMaxSamplesExceeded:
                return "Application holds more samples than configured max.";
            case ComErrc::kNetworkBindingFailure:
                return "Network binding could not be created.";
            case ComErrc::kGrantEnfailed:
                return "Request to grant failed.";
            case ComErrc::kPeerIsUnreachable:
                return "Peer is not reachable.";
            case ComErrc::kFieldValueIsNotValid:
                return "Field value is not valid.";
            case ComErrc::kSetHandlerNotSet:
                return "SetHandler has not been registered.";
            case ComErrc::kSampleAllocationFailure:
                return "Not enough memory for sample allocation.";
            case ComErrc::kIllegalUseOfAllocate:
                return "Illegal use of Allocate API.";
            case ComErrc::kServiceNotOffered:
                return "Service is not offered.";
            case ComErrc::kCommunicationLinkError:
                return "Communication link error.";
            case ComErrc::kNoClients:
                return "No clients connected.";
            case ComErrc::kCommunicationStackError:
                return "Communication stack error.";
            case ComErrc::kInstanceIDCouldNotBeResolved:
                return "InstanceID could not be resolved.";
            default:
                return "Unknown communication error.";
            }
        }

        core::ErrorCode MakeErrorCode(ComErrc code) noexcept
        {
            static const ComErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
