#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        PerErrorDomain::PerErrorDomain() noexcept : ErrorDomain{cDomainId}
        {
        }

        const char *PerErrorDomain::Name() const noexcept
        {
            return "Per";
        }

        const char *PerErrorDomain::Message(
            core::ErrorDomain::CodeType errorCode) const noexcept
        {
            PerErrc _code{static_cast<PerErrc>(errorCode)};

            switch (_code)
            {
            case PerErrc::kPhysicalStorageFailure:
                return "Physical storage hardware error.";
            case PerErrc::kIntegrityCorrupted:
                return "Storage integrity check failed.";
            case PerErrc::kValidationFailed:
                return "Data validation failed.";
            case PerErrc::kEncryptionFailed:
                return "Encryption/decryption failed.";
            case PerErrc::kResourceBusy:
                return "Resource is currently in use.";
            case PerErrc::kOutOfStorageSpace:
                return "Not enough storage space.";
            case PerErrc::kKeyNotFound:
                return "Requested key does not exist.";
            case PerErrc::kIllegalWriteAccess:
                return "Write access not permitted.";
            case PerErrc::kInitFailed:
                return "Initialization of storage failed.";
            case PerErrc::kNotInitialized:
                return "Storage not initialized.";
            default:
                return "Unknown persistency error.";
            }
        }

        core::ErrorCode MakeErrorCode(PerErrc code) noexcept
        {
            static const PerErrorDomain cDomain;
            return core::ErrorCode{
                static_cast<core::ErrorDomain::CodeType>(code),
                cDomain};
        }
    }
}
