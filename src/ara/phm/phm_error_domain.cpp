/// @file src/ara/phm/phm_error_domain.cpp
/// @brief Implementation for PHM error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        const core::ErrorDomain::IdType PhmErrorDomain::cDomainId;

        PhmErrorDomain::PhmErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *PhmErrorDomain::Name() const noexcept
        {
            return "Platform health management error domain";
        }

        const char *PhmErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            const auto cError{static_cast<PhmErrc>(errorCode)};
            switch (cError)
            {
            case PhmErrc::kInvalidArgument:
                return "Invalid argument";
            case PhmErrc::kCheckpointCommunicationError:
                return "Checkpoint communication error";
            case PhmErrc::kAlreadyExists:
                return "Entry already exists";
            case PhmErrc::kNotFound:
                return "Entry not found";
            default:
                return "Unsupported PHM error code";
            }
        }

        core::ErrorCode MakeErrorCode(PhmErrc code) noexcept
        {
            static const PhmErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
