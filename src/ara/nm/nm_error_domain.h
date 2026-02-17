/// @file src/ara/nm/nm_error_domain.h
/// @brief Error domain for Network Management.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef NM_ERROR_DOMAIN_H
#define NM_ERROR_DOMAIN_H

#include <cstdint>
#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace nm
    {
        enum class NmErrc : std::int32_t
        {
            kSuccess = 0,
            kNotInitialized = 1,
            kInvalidChannel = 2,
            kAlreadyStarted = 3,
            kNotStarted = 4,
            kInvalidState = 5,
            kTimeout = 6,
            kCoordinatorError = 7,
            kTransportError = 8,
            kChannelBusy = 9
        };

        class NmErrorDomain final : public core::ErrorDomain
        {
        public:
            constexpr NmErrorDomain() noexcept
                : core::ErrorDomain{0x8000'0000'0000'0060ULL}
            {
            }

            const char *Name() const noexcept override
            {
                return "Nm";
            }

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override
            {
                switch (static_cast<NmErrc>(errorCode))
                {
                case NmErrc::kSuccess:
                    return "Success";
                case NmErrc::kNotInitialized:
                    return "Not initialized";
                case NmErrc::kInvalidChannel:
                    return "Invalid channel";
                case NmErrc::kAlreadyStarted:
                    return "Already started";
                case NmErrc::kNotStarted:
                    return "Not started";
                case NmErrc::kInvalidState:
                    return "Invalid state";
                case NmErrc::kTimeout:
                    return "Timeout";
                case NmErrc::kCoordinatorError:
                    return "Coordinator operation failed";
                case NmErrc::kTransportError:
                    return "NM transport I/O error";
                case NmErrc::kChannelBusy:
                    return "Channel is busy";
                default:
                    return "Unknown NM error";
                }
            }
        };

        namespace internal
        {
            inline constexpr NmErrorDomain gNmErrorDomain{};
        }

        inline constexpr const core::ErrorDomain &GetNmErrorDomain() noexcept
        {
            return internal::gNmErrorDomain;
        }

        inline core::ErrorCode MakeErrorCode(NmErrc code) noexcept
        {
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                GetNmErrorDomain()};
        }
    }
}

#endif
