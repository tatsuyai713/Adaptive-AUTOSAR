#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include "../../../../src/ara/com/option/option_deserializer.h"
#include "../../../../src/ara/com/option/ipv6_endpoint_option.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            TEST(Ipv6EndpointOptionTest, UnicastFactory)
            {
                const bool cDiscardable = true;
                const helper::Ipv6Address cIpAddress("2001:db8::1");
                const Layer4ProtocolType cProtocol = Layer4ProtocolType::Tcp;
                const uint16_t cPort = 8080;
                const OptionType cType = OptionType::IPv6Endpoint;

                auto _option =
                    Ipv6EndpointOption::CreateUnicastEndpoint(
                        cDiscardable, cIpAddress, cProtocol, cPort);

                EXPECT_EQ(_option->Discardable(), cDiscardable);
                EXPECT_EQ(_option->IpAddress(), cIpAddress);
                EXPECT_EQ(_option->L4Proto(), cProtocol);
                EXPECT_EQ(_option->Port(), cPort);
                EXPECT_EQ(_option->Type(), cType);
            }

            TEST(Ipv6EndpointOptionTest, MulticastFactory)
            {
                const bool cDiscardable = false;
                const helper::Ipv6Address cNonMulticast("2001:db8::1");
                const uint16_t cPort = 8090;

                EXPECT_THROW(
                    Ipv6EndpointOption::CreateMulticastEndpoint(
                        cDiscardable, cNonMulticast, cPort),
                    std::invalid_argument);
            }

            TEST(Ipv6EndpointOptionTest, ValidMulticastFactory)
            {
                const bool cDiscardable = true;
                // ff02::1 is a valid IPv6 multicast address
                const helper::Ipv6Address cMulticast(
                    0xff, 0x02, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01);
                const uint16_t cPort = 30490;
                const OptionType cType = OptionType::IPv6Multicast;

                auto _option =
                    Ipv6EndpointOption::CreateMulticastEndpoint(
                        cDiscardable, cMulticast, cPort);

                EXPECT_EQ(_option->Type(), cType);
                EXPECT_EQ(_option->IpAddress(), cMulticast);
                EXPECT_EQ(_option->Port(), cPort);
                EXPECT_EQ(_option->L4Proto(), Layer4ProtocolType::Udp);
            }

            TEST(Ipv6EndpointOptionTest, SdEndpointFactory)
            {
                const bool cDiscardable = false;
                const helper::Ipv6Address cIpAddress("::1");
                const OptionType cType = OptionType::IPv6SdEndpoint;

                auto _option =
                    Ipv6EndpointOption::CreateSdEndpoint(
                        cDiscardable, cIpAddress);

                EXPECT_EQ(_option->Type(), cType);
                EXPECT_EQ(_option->L4Proto(), Layer4ProtocolType::Udp);
                EXPECT_EQ(_option->Port(), 30490U);
            }

            TEST(Ipv6EndpointOptionTest, LengthMethod)
            {
                const helper::Ipv6Address cIpAddress("::1");

                auto _option =
                    Ipv6EndpointOption::CreateUnicastEndpoint(
                        false, cIpAddress, Layer4ProtocolType::Tcp, 80);

                // IPv6 option length = 16 (addr) + 1 (reserved) + 1 (proto) + 2 (port) + 1 (extra) = 21
                EXPECT_EQ(_option->Length(), 21U);
            }

            TEST(Ipv6EndpointOptionTest, PayloadRoundTrip)
            {
                const bool cDiscardable = true;
                const helper::Ipv6Address cIpAddress(
                    0xfe, 0x80, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01);
                const Layer4ProtocolType cProtocol = Layer4ProtocolType::Tcp;
                const uint16_t cPort = 443;

                auto _original =
                    Ipv6EndpointOption::CreateUnicastEndpoint(
                        cDiscardable, cIpAddress, cProtocol, cPort);

                auto _payload = _original->Payload();

                // Payload should be: 2 (length) + 1 (type) + 1 (discardable)
                //                   + 16 (addr) + 1 (reserved) + 1 (proto) + 2 (port) = 24
                EXPECT_EQ(_payload.size(), 24U);
            }

            TEST(Ipv6EndpointOptionTest, DeserializeUnicast)
            {
                const bool cDiscardable = true;
                const helper::Ipv6Address cIpAddress(
                    0x20, 0x01, 0x0d, 0xb8,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01);
                const Layer4ProtocolType cProtocol = Layer4ProtocolType::Tcp;
                const uint16_t cPort = 8080;

                auto _original =
                    Ipv6EndpointOption::CreateUnicastEndpoint(
                        cDiscardable, cIpAddress, cProtocol, cPort);

                auto _payload = _original->Payload();
                std::size_t _offset = 0;
                auto _deserializedBase =
                    OptionDeserializer::Deserialize(_payload, _offset);

                auto _deserialized =
                    dynamic_cast<Ipv6EndpointOption *>(
                        _deserializedBase.get());

                ASSERT_NE(_deserialized, nullptr);

                EXPECT_EQ(_original->Type(), _deserialized->Type());
                EXPECT_EQ(_original->Discardable(), _deserialized->Discardable());
                EXPECT_EQ(_original->IpAddress(), _deserialized->IpAddress());
                EXPECT_EQ(_original->L4Proto(), _deserialized->L4Proto());
                EXPECT_EQ(_original->Port(), _deserialized->Port());
            }

            TEST(Ipv6EndpointOptionTest, DeserializeSdEndpoint)
            {
                const bool cDiscardable = false;
                const helper::Ipv6Address cIpAddress("::1");

                auto _original =
                    Ipv6EndpointOption::CreateSdEndpoint(
                        cDiscardable, cIpAddress);

                auto _payload = _original->Payload();
                std::size_t _offset = 0;
                auto _deserializedBase =
                    OptionDeserializer::Deserialize(_payload, _offset);

                auto _deserialized =
                    dynamic_cast<Ipv6EndpointOption *>(
                        _deserializedBase.get());

                ASSERT_NE(_deserialized, nullptr);
                EXPECT_EQ(_deserialized->Type(), OptionType::IPv6SdEndpoint);
            }

            TEST(Ipv6EndpointOptionTest, DeserializeInvalidTypeThrows)
            {
                const helper::Ipv6Address cIpAddress("::1");

                // Build a payload by hand for an IPv4 type — should throw
                std::vector<uint8_t> dummyPayload(20, 0);
                std::size_t offset = 0;

                EXPECT_THROW(
                    Ipv6EndpointOption::Deserialize(
                        dummyPayload, offset,
                        OptionType::IPv4Endpoint, false),
                    std::out_of_range);
            }
        }
    }
}
