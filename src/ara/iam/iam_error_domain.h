/// @file src/ara/iam/iam_error_domain.h
/// @brief Declarations for iam error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef IAM_ERROR_DOMAIN_H
#define IAM_ERROR_DOMAIN_H

#include "../core/error_code.h"
#include "../core/error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Error codes for ara::iam subset implementation.
        enum class IamErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidArgument = 1,
            kPolicyStoreError = 2,
            kPolicyFileParseError = 3,  ///< Policy file format is invalid
            kGrantNotFound = 4,        ///< Requested grant does not exist
            kGrantExpired = 5,         ///< Grant has expired
            kGrantRevoked = 6,         ///< Grant has been revoked
            kVersionNotFound = 7,      ///< Requested policy version does not exist
            kSnapshotError = 8         ///< Policy snapshot operation failed
        };

        /// @brief Error domain for ara::iam subset implementation.
        class IamErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000701};

        public:
            IamErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::iam domain.
        /// @param code IAM error code.
        /// @returns Error code bound to IamErrorDomain.
        core::ErrorCode MakeErrorCode(IamErrc code) noexcept;
    }
}

#endif
