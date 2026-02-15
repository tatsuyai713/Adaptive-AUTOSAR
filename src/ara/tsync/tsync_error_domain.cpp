/// @file src/ara/tsync/tsync_error_domain.cpp
/// @brief Implementation for tsync error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        TsyncErrorDomain::TsyncErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *TsyncErrorDomain::Name() const noexcept
        {
            return "Tsync";
        }

        const char *TsyncErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            TsyncErrc _code{static_cast<TsyncErrc>(errorCode)};
            switch (_code)
            {
            case TsyncErrc::kNotSynchronized:
                return "Time base is not synchronized.";
            case TsyncErrc::kInvalidArgument:
                return "Invalid time synchronization argument.";
            default:
                return "Unknown time synchronization error.";
            }
        }

        core::ErrorCode MakeErrorCode(TsyncErrc code) noexcept
        {
            static const TsyncErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
