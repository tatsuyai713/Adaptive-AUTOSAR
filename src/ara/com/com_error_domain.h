#ifndef COM_ERROR_DOMAIN_H
#define COM_ERROR_DOMAIN_H

#include "../core/error_domain.h"
#include "../core/error_code.h"

namespace ara
{
    namespace com
    {
        /// @brief Communication Management error codes per AUTOSAR AP
        enum class ComErrc : ara::core::ErrorDomain::CodeType
        {
            kServiceNotAvailable = 1,      ///< Service is not available
            kMaxSamplesExceeded = 2,       ///< Application holds more samples than configured max
            kNetworkBindingFailure = 3,    ///< Network binding could not be created
            kGrantEnfailed = 4,            ///< Request to grant failed
            kPeerIsUnreachable = 5,        ///< Peer is not reachable
            kFieldValueIsNotValid = 6,     ///< Field value is not valid
            kSetHandlerNotSet = 7,         ///< SetHandler has not been registered
            kSampleAllocationFailure = 8,  ///< Not enough memory for sample allocation
            kIllegalUseOfAllocate = 9,     ///< Illegal use of Allocate API
            kServiceNotOffered = 10,       ///< Service is not offered
            kCommunicationLinkError = 11,  ///< Communication link error
            kNoClients = 12,               ///< No clients connected
            kCommunicationStackError = 13, ///< Communication stack error
            kInstanceIDCouldNotBeResolved = 14 ///< InstanceID could not be resolved
        };

        /// @brief Communication Management ErrorDomain
        class ComErrorDomain final : public core::ErrorDomain
        {
        private:
            static const core::ErrorDomain::IdType cDomainId{0x8000000000000201};

        public:
            ComErrorDomain() noexcept;

            const char *Name() const noexcept override;

            const char *Message(
                core::ErrorDomain::CodeType errorCode) const noexcept override;
        };

        /// @brief Create ara::core::ErrorCode in ara::com domain
        /// @param code Communication error code
        /// @returns Error code bound to ComErrorDomain
        core::ErrorCode MakeErrorCode(ComErrc code) noexcept;
    }
}

#endif
