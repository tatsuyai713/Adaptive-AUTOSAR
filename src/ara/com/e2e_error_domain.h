/// @file src/ara/com/e2e_error_domain.h
/// @brief E2E protection error domain per SWS_CM_01000+ / SWS_E2E_00401.
/// @details Provides specific error codes for E2E protection failures,
///          reported through the ara::core::ErrorDomain mechanism.

#ifndef ARA_COM_E2E_ERROR_DOMAIN_H
#define ARA_COM_E2E_ERROR_DOMAIN_H

#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            /// @brief Error codes for E2E protection per SWS_E2E_00401.
            enum class E2EErrc : ara::core::ErrorDomain::CodeType
            {
                kRepeated = 1,         ///< Counter was repeated (possible replay).
                kWrongSequence = 2,    ///< Counter indicates missing/out-of-order data.
                kError = 3,            ///< CRC mismatch detected.
                kNotAvailable = 4,     ///< No data received within timeout.
                kNoNewData = 5,        ///< Data identical to last received.
                kStateMachineError = 6 ///< E2E state machine reported invalid state.
            };

            /// @brief ErrorDomain implementation for E2E protection.
            class E2EErrorDomain final : public core::ErrorDomain
            {
            private:
                static const core::ErrorDomain::IdType cDomainId{
                    0x8000000000000202};

            public:
                /// @brief Constructs the E2E error domain.
                E2EErrorDomain() noexcept : core::ErrorDomain{cDomainId} {}

                /// @brief Returns the domain short name.
                const char *Name() const noexcept override
                {
                    return "E2E";
                }

                /// @brief Converts an error code to a human-readable message.
                const char *Message(
                    core::ErrorDomain::CodeType errorCode) const noexcept override
                {
                    switch (static_cast<E2EErrc>(errorCode))
                    {
                    case E2EErrc::kRepeated:
                        return "E2E: counter repeated";
                    case E2EErrc::kWrongSequence:
                        return "E2E: wrong sequence";
                    case E2EErrc::kError:
                        return "E2E: CRC mismatch";
                    case E2EErrc::kNotAvailable:
                        return "E2E: data not available";
                    case E2EErrc::kNoNewData:
                        return "E2E: no new data";
                    case E2EErrc::kStateMachineError:
                        return "E2E: state machine error";
                    default:
                        return "E2E: unknown error";
                    }
                }

                /// @brief Throw the error code as an exception.
                void ThrowAsException(
                    const core::ErrorCode &) const override
                {
                    // Educational: no-op
                }
            };

            /// @brief Creates an ErrorCode in the E2EErrorDomain.
            /// @param code E2E error code value.
            /// @returns Typed error code.
            inline core::ErrorCode MakeErrorCode(E2EErrc code) noexcept
            {
                static const E2EErrorDomain sDomain;
                return core::ErrorCode{
                    static_cast<core::ErrorDomain::CodeType>(code),
                    sDomain};
            }
        } // namespace e2e
    }
}

#endif
