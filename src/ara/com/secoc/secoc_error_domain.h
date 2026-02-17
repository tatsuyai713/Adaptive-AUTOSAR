/// @file src/ara/com/secoc/secoc_error_domain.h
/// @brief Error domain for ara::com::secoc (Secure Onboard Communication).
/// @details AUTOSAR Secure Onboard Communication (SecOC) provides message
///          authentication for CAN, LIN, and FlexRay communications.
///          This file defines the error domain used by the SecOC implementation.
///
///          Reference: AUTOSAR_SWS_SecureOnboardCommunication
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SECOC_ERROR_DOMAIN_H
#define ARA_COM_SECOC_ERROR_DOMAIN_H

#include "../../core/error_domain.h"
#include "../../core/error_code.h"
#include "../../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Secure Onboard Communication (SecOC) namespace.
        /// @details Implements MAC-based authentication for vehicle bus messages
        ///          following AUTOSAR SecOC specification.
        namespace secoc
        {
            /// @brief Error codes for SecOC operations.
            enum class SecOcErrc : ara::core::ErrorDomain::CodeType
            {
                kAuthenticationFailed = 1,  ///< MAC verification failed
                kFreshnessCounterFailed = 2, ///< Freshness value mismatch or replay detected
                kKeyNotFound = 3,           ///< Required key not available
                kInvalidPayloadLength = 4,  ///< Payload size is invalid
                kTruncatedMacFailed = 5,    ///< Truncated MAC length mismatch
                kFreshnessOverflow = 6,     ///< Freshness counter overflow
                kNotInitialized = 7,        ///< SecOC instance not initialized
                kConfigurationError = 8     ///< Configuration is invalid
            };

            /// @brief Error domain for SecOC operations.
            class SecOcErrorDomain final : public ara::core::ErrorDomain
            {
            private:
                static constexpr ErrorDomain::IdType cId{0x80000000000001A0ULL};

            public:
                using Errc = SecOcErrc;

                constexpr SecOcErrorDomain() noexcept : ErrorDomain{cId} {}

                const char *Name() const noexcept override
                {
                    return "SecOC";
                }

                const char *Message(ara::core::ErrorDomain::CodeType errorCode) const noexcept override
                {
                    switch (static_cast<Errc>(errorCode))
                    {
                    case Errc::kAuthenticationFailed:
                        return "MAC authentication failed";
                    case Errc::kFreshnessCounterFailed:
                        return "Freshness counter verification failed";
                    case Errc::kKeyNotFound:
                        return "Cryptographic key not found";
                    case Errc::kInvalidPayloadLength:
                        return "Invalid payload length";
                    case Errc::kTruncatedMacFailed:
                        return "Truncated MAC length mismatch";
                    case Errc::kFreshnessOverflow:
                        return "Freshness counter overflow";
                    case Errc::kNotInitialized:
                        return "SecOC instance not initialized";
                    case Errc::kConfigurationError:
                        return "SecOC configuration error";
                    default:
                        return "Unknown SecOC error";
                    }
                }

                void ThrowAsException(const ara::core::ErrorCode &errorCode) const noexcept(false) override
                {
                    throw errorCode;
                }
            };

            namespace internal
            {
                constexpr SecOcErrorDomain cSecOcErrorDomain;
            }

            /// @brief Create an ErrorCode for SecOC errors.
            constexpr ara::core::ErrorCode MakeErrorCode(
                SecOcErrc code,
                ara::core::ErrorDomain::SupportDataType data = 0) noexcept
            {
                return {static_cast<ara::core::ErrorDomain::CodeType>(code),
                        internal::cSecOcErrorDomain,
                        data};
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara

#endif // ARA_COM_SECOC_ERROR_DOMAIN_H
