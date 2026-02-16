/// @file src/ara/sm/sm_error_domain.cpp
/// @brief Implementation for SM error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        const core::ErrorDomain::IdType SmErrorDomain::cDomainId;

        SmErrorDomain::SmErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *SmErrorDomain::Name() const noexcept
        {
            return "SM";
        }

        const char *SmErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            const auto cError{static_cast<SmErrc>(errorCode)};
            switch (cError)
            {
            case SmErrc::kInvalidState:
                return "Operation not permitted in current state.";
            case SmErrc::kTransitionFailed:
                return "State transition failed.";
            case SmErrc::kAlreadyInState:
                return "Already in the requested state.";
            case SmErrc::kNetworkUnavailable:
                return "Network resource is unavailable.";
            case SmErrc::kInvalidArgument:
                return "Invalid argument supplied.";
            default:
                return "Unknown SM error.";
            }
        }

        core::ErrorCode MakeErrorCode(SmErrc code) noexcept
        {
            static const SmErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
