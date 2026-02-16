/// @file src/ara/tsync/tsync_error_domain.h
/// @brief Declarations for tsync error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef TSYNC_ERROR_DOMAIN_H
#define TSYNC_ERROR_DOMAIN_H

#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Error codes for ara::tsync subset implementation.
        enum class TsyncErrc : ara::core::ErrorDomain::CodeType
        {
            kNotSynchronized = 1,
            kInvalidArgument = 2,
            kProviderUnavailable = 3, ///< Time source provider is unavailable.
            kDeviceOpenFailed = 4,    ///< Failed to open time source device.
            kQueryFailed = 5          ///< Failed to query time source.
        };

        /// @brief Error domain for ara::tsync subset implementation.
        class TsyncErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000801};

        public:
            TsyncErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::tsync domain.
        /// @param code Time sync error code.
        /// @returns Error code bound to TsyncErrorDomain.
        core::ErrorCode MakeErrorCode(TsyncErrc code) noexcept;
    }
}

#endif
