#ifndef PER_ERROR_DOMAIN_H
#define PER_ERROR_DOMAIN_H

#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    /// @brief Adaptive AUTOSAR Persistency functional cluster
    namespace per
    {
        /// @brief Persistency error codes per AUTOSAR AP SWS_PER
        enum class PerErrc : ara::core::ErrorDomain::CodeType
        {
            kPhysicalStorageFailure = 1,  ///< Physical storage hardware error
            kIntegrityCorrupted = 2,      ///< Storage integrity check failed
            kValidationFailed = 3,        ///< Data validation failed
            kEncryptionFailed = 4,        ///< Encryption/decryption failed
            kResourceBusy = 5,            ///< Resource is currently in use
            kOutOfStorageSpace = 6,       ///< Not enough storage space
            kKeyNotFound = 7,             ///< Requested key does not exist
            kIllegalWriteAccess = 8,      ///< Write access not permitted
            kInitFailed = 9,              ///< Initialization of storage failed
            kNotInitialized = 10          ///< Storage not initialized
        };

        /// @brief Persistency ErrorDomain
        class PerErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000301};

        public:
            PerErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::per domain
        /// @param code Persistency error code
        /// @returns Error code bound to PerErrorDomain
        core::ErrorCode MakeErrorCode(PerErrc code) noexcept;
    }
}

#endif
