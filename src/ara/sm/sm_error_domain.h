/// @file src/ara/sm/sm_error_domain.h
/// @brief Declarations for SM error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SM_ERROR_DOMAIN_H
#define SM_ERROR_DOMAIN_H

#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Error codes for ara::sm subset implementation.
        enum class SmErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidState = 1,       ///< Operation not permitted in current state.
            kTransitionFailed = 2,   ///< State transition failed.
            kAlreadyInState = 3,     ///< Already in the requested state.
            kNetworkUnavailable = 4, ///< Network resource is unavailable.
            kInvalidArgument = 5     ///< Invalid argument supplied.
        };

        /// @brief Error domain for ara::sm subset implementation.
        class SmErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000301};

        public:
            SmErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::sm domain.
        /// @param code SM error code.
        /// @returns Error code bound to SmErrorDomain.
        core::ErrorCode MakeErrorCode(SmErrc code) noexcept;
    }
}

#endif
