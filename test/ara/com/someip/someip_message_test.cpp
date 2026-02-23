#include <gtest/gtest.h>
#include <array>
#include <utility>
#include <vector>
#include "../../../../src/ara/com/someip/someip_message.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace
            {
                class TestSomeIpMessage final : public SomeIpMessage
                {
                public:
                    TestSomeIpMessage(
                        uint32_t messageId,
                        uint16_t clientId,
                        uint8_t protocolVersion,
                        uint8_t interfaceVersion,
                        SomeIpMessageType messageType,
                        uint16_t sessionId)
                        : SomeIpMessage(
                              messageId,
                              clientId,
                              protocolVersion,
                              interfaceVersion,
                              messageType,
                              sessionId)
                    {
                    }

                    TestSomeIpMessage(
                        uint32_t messageId,
                        uint16_t clientId,
                        uint8_t protocolVersion,
                        uint8_t interfaceVersion,
                        SomeIpMessageType messageType,
                        SomeIpReturnCode returnCode,
                        uint16_t sessionId)
                        : SomeIpMessage(
                              messageId,
                              clientId,
                              protocolVersion,
                              interfaceVersion,
                              messageType,
                              returnCode,
                              sessionId)
                    {
                    }

                    uint32_t Length() const noexcept override
                    {
                        return 8U;
                    }
                };
            } // namespace

            TEST(SomeIpMessageTest, RequestSideMessageTypesAreAccepted)
            {
                const uint32_t cMessageId{0x00010002};
                const uint16_t cClientId{0x0201};
                const uint16_t cSessionId{1};
                const uint8_t cProtocolVersion{1};
                const uint8_t cInterfaceVersion{1};

                const std::array<SomeIpMessageType, 6> cRequestTypes{
                    SomeIpMessageType::Request,
                    SomeIpMessageType::RequestNoReturn,
                    SomeIpMessageType::Notification,
                    SomeIpMessageType::TpRequest,
                    SomeIpMessageType::TpRequestNoReturn,
                    SomeIpMessageType::TpNotification};

                for (auto messageType : cRequestTypes)
                {
                    TestSomeIpMessage message(
                        cMessageId,
                        cClientId,
                        cProtocolVersion,
                        cInterfaceVersion,
                        messageType,
                        cSessionId);
                    EXPECT_EQ(message.MessageType(), messageType);
                    EXPECT_EQ(message.ReturnCode(), SomeIpReturnCode::eOK);
                }
            }

            TEST(SomeIpMessageTest, ResponseSideMessageTypesAreAccepted)
            {
                const uint32_t cMessageId{0x00010002};
                const uint16_t cClientId{0x0201};
                const uint16_t cSessionId{1};
                const uint8_t cProtocolVersion{1};
                const uint8_t cInterfaceVersion{1};

                const std::vector<std::pair<SomeIpMessageType, SomeIpReturnCode>> cResponseTypes{
                    {SomeIpMessageType::Response, SomeIpReturnCode::eOK},
                    {SomeIpMessageType::TpResponse, SomeIpReturnCode::eOK},
                    {SomeIpMessageType::Error, SomeIpReturnCode::eNotOk},
                    {SomeIpMessageType::TpError, SomeIpReturnCode::eNotOk}};

                for (const auto &entry : cResponseTypes)
                {
                    TestSomeIpMessage message(
                        cMessageId,
                        cClientId,
                        cProtocolVersion,
                        cInterfaceVersion,
                        entry.first,
                        entry.second,
                        cSessionId);
                    EXPECT_EQ(message.MessageType(), entry.first);
                    EXPECT_EQ(message.ReturnCode(), entry.second);
                }
            }

            TEST(SomeIpMessageTest, RequestConstructorRejectsResponseSideTypes)
            {
                const uint32_t cMessageId{0x00010002};
                const uint16_t cClientId{0x0201};
                const uint16_t cSessionId{1};
                const uint8_t cProtocolVersion{1};
                const uint8_t cInterfaceVersion{1};

                const std::array<SomeIpMessageType, 4> cResponseTypes{
                    SomeIpMessageType::Response,
                    SomeIpMessageType::Error,
                    SomeIpMessageType::TpResponse,
                    SomeIpMessageType::TpError};

                for (auto messageType : cResponseTypes)
                {
                    EXPECT_THROW(
                        TestSomeIpMessage(
                            cMessageId,
                            cClientId,
                            cProtocolVersion,
                            cInterfaceVersion,
                            messageType,
                            cSessionId),
                        std::invalid_argument);
                }
            }

            TEST(SomeIpMessageTest, ResponseConstructorRejectsRequestSideTypes)
            {
                const uint32_t cMessageId{0x00010002};
                const uint16_t cClientId{0x0201};
                const uint16_t cSessionId{1};
                const uint8_t cProtocolVersion{1};
                const uint8_t cInterfaceVersion{1};

                const std::array<SomeIpMessageType, 6> cRequestTypes{
                    SomeIpMessageType::Request,
                    SomeIpMessageType::RequestNoReturn,
                    SomeIpMessageType::Notification,
                    SomeIpMessageType::TpRequest,
                    SomeIpMessageType::TpRequestNoReturn,
                    SomeIpMessageType::TpNotification};

                for (auto messageType : cRequestTypes)
                {
                    EXPECT_THROW(
                        TestSomeIpMessage(
                            cMessageId,
                            cClientId,
                            cProtocolVersion,
                            cInterfaceVersion,
                            messageType,
                            SomeIpReturnCode::eNotOk,
                            cSessionId),
                        std::invalid_argument);
                }
            }

            TEST(SomeIpMessageTest, ErrorMessageTypesRejectOkReturnCode)
            {
                const uint32_t cMessageId{0x00010002};
                const uint16_t cClientId{0x0201};
                const uint16_t cSessionId{1};
                const uint8_t cProtocolVersion{1};
                const uint8_t cInterfaceVersion{1};

                EXPECT_THROW(
                    TestSomeIpMessage(
                        cMessageId,
                        cClientId,
                        cProtocolVersion,
                        cInterfaceVersion,
                        SomeIpMessageType::Error,
                        SomeIpReturnCode::eOK,
                        cSessionId),
                    std::invalid_argument);

                EXPECT_THROW(
                    TestSomeIpMessage(
                        cMessageId,
                        cClientId,
                        cProtocolVersion,
                        cInterfaceVersion,
                        SomeIpMessageType::TpError,
                        SomeIpReturnCode::eOK,
                        cSessionId),
                    std::invalid_argument);
            }
        }
    }
}
