/// @file src/ara/com/someip/someip_message.h
/// @brief Declarations for someip message.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SOMEIP_MESSAGE_H
#define SOMEIP_MESSAGE_H

#include <stdint.h>
#include <stdexcept>
#include <vector>
#include <limits>
#include "../helper/payload_helper.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief SOME/IP communication message type
            enum class SomeIpMessageType : uint8_t
            {
                Request = 0x00,           ///< Request expecting a response
                RequestNoReturn = 0x01,   ///< Fire and forget request
                Notification = 0x02,      ///< Notification/event callback
                Response = 0x80,          ///< Response without any error
                Error = 0x81,             ///< Response containing error
                TpRequest = 0x20,         ///< Transfer protocol request
                TpRequestNoReturn = 0x21, ///< Transfer protocol fire and forget request
                TpNotification = 0x22,    ///< Transfer protocol notification
                TpResponse = 0xa0,        ///< Transfer protocol response
                TpError = 0xa1            ///< Transfer protocol error
            };

            /// @brief SOME/IP communication message return code
            enum class SomeIpReturnCode : uint8_t
            {
                eOK,                    ///< No error occurred
                eNotOk,                 ///< Unspecified error occurred
                eUnknownService,        ///< Service ID is unknown
                eUnknownMethod,         ///< Method ID is unknown
                eNotReady,              ///< Service is not running
                eNotReachable,          ///< Service is not reachable
                eTimeout,               ///< Timeout occurred
                eWrongProtocolVersion,  ///< SOME/IP protocol version is not supported
                eWrongInterfaceVersion, ///< Service interface version is not supported
                eMalformedMessage,      ///< Deserialization error occurred
                eWrongMessageType,      ///< Invalid message type
                eE2eRepeated,           ///< Repeated E2E calculation error
                eE2eWrongSequnece,      ///< Wrong E2E sequence error
                eE2e,                   ///< Unspecified E2E error
                eE2eNotAvailable,       ///< E2E is not supported
                eE2eNoNewData           ///< No new data E2E calculation present
            };

            /// @brief SOME/IP Abstract Message
            class SomeIpMessage
            {
            private:
                uint32_t mMessageId;
                uint16_t mClientId;
                uint16_t mSessionId;
                uint8_t mProtocolVersion;
                uint8_t mInterfaceVersion;
                SomeIpMessageType mMessageType;
                SomeIpReturnCode mReturnCode;

                /// @brief SOME/IP-TP header: offset [31:4] + more-segments flag [0].
                ///        Only meaningful when MessageType is a TP type.
                uint32_t mTpHeader{0U};

                SomeIpMessage(uint32_t messageId,
                              uint16_t clientId,
                              uint16_t sessionId,
                              uint8_t protocolVersion,
                              uint8_t interfaceVersion,
                              SomeIpMessageType messageType,
                              SomeIpReturnCode returnCode) noexcept;

            protected:
                /// @brief Constructor for request and notification
                /// @param messageId Message ID consisting service and method/event ID
                /// @param clientId Client ID including ID prefix
                /// @param protocolVersion SOME/IP protocol header version
                /// @param interfaceVersion Service interface version
                /// @param messageType SOME/IP message type (request or notification)
                /// @param sessionId Active/non-active session ID
                SomeIpMessage(uint32_t messageId,
                              uint16_t clientId,
                              uint8_t protocolVersion,
                              uint8_t interfaceVersion,
                              SomeIpMessageType messageType,
                              uint16_t sessionId = 1);

                /// @brief Constructor for response and error
                /// @param messageId Message ID consisting service and method/event ID
                /// @param clientId Client ID including ID prefix
                /// @param protocolVersion SOME/IP protocol header version
                /// @param interfaceVersion Service interface version
                /// @param messageType SOME/IP message type (response or error)
                /// @param returnCode Message response/error return code
                /// @param sessionId Active/non-active session ID
                SomeIpMessage(uint32_t messageId,
                              uint16_t clientId,
                              uint8_t protocolVersion,
                              uint8_t interfaceVersion,
                              SomeIpMessageType messageType,
                              SomeIpReturnCode returnCode,
                              uint16_t sessionId = 1);

                /// @brief Deserialize a SOME/IP message payload
                /// @param message SOME/IP message to be filled by deserializing the payload
                /// @param payload Serialized SOME/IP message payload byte array
                static void Deserialize(
                    SomeIpMessage *message,
                    const std::vector<uint8_t> &payload);

            public:
                SomeIpMessage(SomeIpMessage&& other) noexcept;
                virtual ~SomeIpMessage() noexcept = default;

                SomeIpMessage& operator=(SomeIpMessage&& other);

                /// @brief Get message ID
                /// @returns Message ID consisting service and method/event ID
                uint32_t MessageId() const noexcept;

                /// @brief Get message length
                /// @returns Message length including the payload length
                virtual uint32_t Length() const noexcept = 0;

                /// @brief Get client ID
                /// @returns Client ID including ID prefix
                uint16_t ClientId() const noexcept;

                /// @brief Get session ID
                /// @returns Active/non-active session ID
                uint16_t SessionId() const noexcept;

                /// @brief Set a new session ID
                /// @param sessionId New session ID
                virtual void SetSessionId(uint16_t sessionId);

                /// @brief Increment the session ID by one
                /// @returns True if the session ID is wrappered; otherwise false
                /// @note In the case of wrapping, session ID will start from 1
                virtual bool IncrementSessionId() noexcept;

                /// @brief Get protocol version
                /// @returns SOME/IP protocol header version
                uint8_t ProtocolVersion() const noexcept;

                /// @brief Get interface version
                /// @returns Service interface version
                uint8_t InterfaceVersion() const noexcept;

                /// @brief Get message type
                /// @returns SOME/IP message type
                SomeIpMessageType MessageType() const noexcept;

                /// @brief Get return code
                /// @returns SOME/IP message return code
                SomeIpReturnCode ReturnCode() const noexcept;

                /// @brief Check whether this message uses the SOME/IP-TP protocol.
                /// @returns `true` when the message type has the TP flag set.
                bool IsTp() const noexcept;

                /// @brief Get the SOME/IP-TP byte offset (bits [31:4] of the TP header).
                /// @returns Byte offset of this segment within the reassembled payload.
                uint32_t TpOffset() const noexcept;

                /// @brief Get the SOME/IP-TP "more segments" flag (bit 0 of the TP header).
                /// @returns `true` when more segments follow.
                bool TpMoreSegments() const noexcept;

                /// @brief Set the raw SOME/IP-TP header (offset [31:4] + more-flag [0]).
                /// @param tpHeader Raw 32-bit TP header value
                void SetTpHeader(uint32_t tpHeader) noexcept;

                /// @brief Build and set the TP header from offset and more-segments flag.
                /// @param offset Byte offset within the reassembled payload (must be
                ///        a multiple of 16)
                /// @param moreSegments True if more segments follow
                void SetTpFields(uint32_t offset, bool moreSegments) noexcept;

                /// @brief Get message payload
                /// @returns Byte array
                virtual std::vector<uint8_t> Payload() const;
            };
        }
    }
}

#endif