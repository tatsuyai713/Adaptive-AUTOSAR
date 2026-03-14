/// @file src/ara/com/option/ipv6_endpoint_option.h
/// @brief Declarations for ipv6 endpoint option.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef IPV6_ENDPOINT_OPTION_H
#define IPV6_ENDPOINT_OPTION_H

#include <stdexcept>
#include <memory>
#include "./option.h"
#include "../helper/ipv6_address.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            /// @brief IPv6 endpoint option for both generic and service discovery purposes
            class Ipv6EndpointOption : public Option
            {
            private:
                static const Layer4ProtocolType cDefaultSdProtocol = Layer4ProtocolType::Udp;
                static const uint16_t cDefaultSdPort = 30490;

                helper::Ipv6Address mIpAddress;
                Layer4ProtocolType mL4Proto;
                uint16_t mPort;

                Ipv6EndpointOption(
                    OptionType type,
                    bool discardable,
                    helper::Ipv6Address ipAddress,
                    Layer4ProtocolType protocol,
                    uint16_t port) noexcept;

            public:
                Ipv6EndpointOption() = delete;
                virtual uint16_t Length() const noexcept override;

                /// @brief Get IP address
                /// @returns IPv6 address
                helper::Ipv6Address IpAddress() const noexcept;

                /// @brief Get protocol
                /// @returns OSI layer-4 protocol
                Layer4ProtocolType L4Proto() const noexcept;

                /// @brief Get port
                /// @returns Network port number
                uint16_t Port() const noexcept;

                virtual std::vector<uint8_t> Payload() const override;

                /// @brief Unicast endpoint factory
                /// @param discardable Indicates whether the option can be discarded or not
                /// @param ipAddress IPv6 address
                /// @param protocol Layer-4 protocol
                /// @param port Port number
                /// @returns Unicast IPv6 endpoint
                static std::unique_ptr<Ipv6EndpointOption> CreateUnicastEndpoint(
                    bool discardable,
                    helper::Ipv6Address ipAddress,
                    Layer4ProtocolType protocol,
                    uint16_t port) noexcept;

                /// @brief Multicast endpoint factory
                /// @param discardable Indicates whether the option can be discarded or not
                /// @param ipAddress IPv6 multicast address (must start with ff)
                /// @param port Port number
                /// @returns Multicast IPv6 endpoint
                static std::unique_ptr<Ipv6EndpointOption> CreateMulticastEndpoint(
                    bool discardable,
                    helper::Ipv6Address ipAddress,
                    uint16_t port);

                /// @brief Service discovery factory
                /// @param discardable Indicates whether the option can be discarded or not
                /// @param ipAddress IPv6 address
                /// @param protocol Layer-4 protocol
                /// @param port Port number
                /// @returns Service discovery IPv6 endpoint
                static std::unique_ptr<Ipv6EndpointOption> CreateSdEndpoint(
                    bool discardable,
                    helper::Ipv6Address ipAddress,
                    Layer4ProtocolType protocol = cDefaultSdProtocol,
                    uint16_t port = cDefaultSdPort) noexcept;

                /// @brief Deserialize an option payload
                /// @param payload Serialized option payload byte array
                /// @param offset Deserializing offset in the payload
                /// @param type IPv6 endpoint option type
                /// @param discardable Indicates whether the option can be discarded or not
                /// @returns Deserialized option
                /// @throws std::out_of_range Throws when the option type is not an IPv6 endpoint
                static std::unique_ptr<Ipv6EndpointOption> Deserialize(
                    const std::vector<uint8_t> &payload,
                    std::size_t &offset,
                    OptionType type,
                    bool discardable);
            };
        }
    }
}

#endif
