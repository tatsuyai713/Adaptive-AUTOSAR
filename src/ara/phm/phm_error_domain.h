/// @file src/ara/phm/phm_error_domain.h
/// @brief Declarations for PHM error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PHM_ERROR_DOMAIN_H
#define PHM_ERROR_DOMAIN_H

#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace phm
    {
        /// @brief Platform health management error codes.
        enum class PhmErrc : ara::core::ErrorDomain::CodeType
        {
            kInvalidArgument = 1,             ///< Invalid input argument.
            kCheckpointCommunicationError = 2,///< Checkpoint could not be sent.
            kAlreadyExists = 3,               ///< Entity with same key already exists.
            kNotFound = 4,                    ///< Requested entity was not found.
            kAlreadyOffered = 5,              ///< Service is already offered.
            kNotOffered = 6                   ///< Service is not offered yet.
        };

        /// @brief Error domain implementation for PHM.
        class PhmErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000501};

        public:
            /// @brief Constructor.
            PhmErrorDomain() noexcept;

            /// @brief Domain short name.
            const char *Name() const noexcept override;

            /// @brief Message conversion for PHM error codes.
            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create `ara::core::ErrorCode` in PHM domain.
        /// @param code PHM error enum value.
        /// @returns Error code in `PhmErrorDomain`.
        core::ErrorCode MakeErrorCode(PhmErrc code) noexcept;
    }
}

#endif
