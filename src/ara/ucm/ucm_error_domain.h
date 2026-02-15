#ifndef UCM_ERROR_DOMAIN_H
#define UCM_ERROR_DOMAIN_H

#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief Error codes for ara::ucm subset implementation.
        enum class UcmErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidArgument = 1,
            kInvalidState = 2,
            kNoActiveSession = 3,
            kPackageNotStaged = 4,
            kVerificationFailed = 5,
            kSessionCancelled = 6,
            kDowngradeNotAllowed = 7,
            kClusterNotFound = 8
        };

        /// @brief Error domain for ara::ucm subset implementation.
        class UcmErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000901};

        public:
            UcmErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::ucm domain.
        /// @param code UCM error code.
        /// @returns Error code bound to UcmErrorDomain.
        core::ErrorCode MakeErrorCode(UcmErrc code) noexcept;
    }
}

#endif
