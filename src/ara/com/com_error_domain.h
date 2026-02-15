/// @file src/ara/com/com_error_domain.h
/// @brief Declarations for com error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef COM_ERROR_DOMAIN_H
#define COM_ERROR_DOMAIN_H

#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace com
    {
        /// @brief Error codes used by the `ara::com` abstraction layer.
        /// @details
        /// Values map transport/runtime failures into `ara::core::ErrorCode`
        /// so that Proxy/Skeleton APIs can report errors in a uniform way.
        enum class ComErrc : ara::core::ErrorDomain::CodeType
        {
            kServiceNotAvailable = 1,      ///< Target service instance is currently unavailable.
            kMaxSamplesExceeded = 2,       ///< Consumer retained more samples than the configured limit.
            kNetworkBindingFailure = 3,    ///< Transport/binding object creation failed.
            kGrantEnfailed = 4,            ///< Middleware could not grant requested operation/resource.
            kPeerIsUnreachable = 5,        ///< Remote peer cannot be reached on the selected transport.
            kFieldValueIsNotValid = 6,     ///< Received or provided field value is invalid.
            kSetHandlerNotSet = 7,         ///< Required set-handler callback was not configured.
            kSampleAllocationFailure = 8,  ///< Sample memory allocation failed.
            kIllegalUseOfAllocate = 9,     ///< Allocate API was used in an invalid state/context.
            kServiceNotOffered = 10,       ///< Skeleton attempted operation while not offered.
            kCommunicationLinkError = 11,  ///< Communication link level error occurred.
            kNoClients = 12,               ///< Operation requires at least one connected client.
            kCommunicationStackError = 13, ///< Underlying communication stack reported an error.
            kInstanceIDCouldNotBeResolved = 14 ///< Instance identifier resolution failed.
        };

        /// @brief `ErrorDomain` implementation for `ara::com`.
        class ComErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000201};

        public:
            /// @brief Constructs the fixed `ara::com` error domain.
            ComErrorDomain() noexcept;

            /// @brief Returns the domain short name.
            /// @returns Constant domain name string.
            const char *Name() const noexcept override;

            /// @brief Converts a domain code to a human-readable message.
            /// @param errorCode Value from `ComErrc`.
            /// @returns Constant message string for logging and diagnostics.
            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Creates an `ara::core::ErrorCode` in the `ComErrorDomain`.
        /// @param code `ara::com` error code value.
        /// @returns Typed error code that can be returned from `ara::com` APIs.
        core::ErrorCode MakeErrorCode(ComErrc code) noexcept;
    }
}

#endif
