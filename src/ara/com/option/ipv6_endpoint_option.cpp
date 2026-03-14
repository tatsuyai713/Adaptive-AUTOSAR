/// @file src/ara/com/option/ipv6_endpoint_option.cpp
/// @brief Implementation for ipv6 endpoint option.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./ipv6_endpoint_option.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            Ipv6EndpointOption::Ipv6EndpointOption(
                OptionType type,
                bool discardable,
                helper::Ipv6Address ipAddress,
                Layer4ProtocolType protocol,
                uint16_t port) noexcept
                : Option(type, discardable),
                  mIpAddress{ipAddress},
                  mL4Proto{protocol},
                  mPort{port}
            {
            }

            uint16_t Ipv6EndpointOption::Length() const noexcept
            {
                // 16 bytes IPv6 address + 1 reserved + 1 protocol + 2 port = 20
                // (differs from IPv4 option length of 9)
                const uint16_t cOptionLength = 21;
                return cOptionLength;
            }

            helper::Ipv6Address Ipv6EndpointOption::IpAddress() const noexcept
            {
                return mIpAddress;
            }

            Layer4ProtocolType Ipv6EndpointOption::L4Proto() const noexcept
            {
                return mL4Proto;
            }

            uint16_t Ipv6EndpointOption::Port() const noexcept
            {
                return mPort;
            }

            std::vector<uint8_t> Ipv6EndpointOption::Payload() const
            {
                auto _result = Option::BasePayload();

                helper::Ipv6Address::Inject(_result, mIpAddress);

                const uint8_t cReservedByte = 0x00;
                _result.push_back(cReservedByte);

                uint8_t _protocolByte = static_cast<uint8_t>(mL4Proto);
                _result.push_back(_protocolByte);

                helper::Inject(_result, mPort);

                return _result;
            }

            std::unique_ptr<Ipv6EndpointOption> Ipv6EndpointOption::CreateUnicastEndpoint(
                bool discardable,
                helper::Ipv6Address ipAddress,
                Layer4ProtocolType protocol,
                uint16_t port) noexcept
            {
                std::unique_ptr<Ipv6EndpointOption> _result(
                    new Ipv6EndpointOption(
                        OptionType::IPv6Endpoint,
                        discardable,
                        ipAddress,
                        protocol,
                        port));

                return _result;
            }

            std::unique_ptr<Ipv6EndpointOption> Ipv6EndpointOption::CreateMulticastEndpoint(
                bool discardable,
                helper::Ipv6Address ipAddress,
                uint16_t port)
            {
                // IPv6 multicast addresses start with 0xFF
                const uint8_t cMulticastPrefix = 0xFF;
                if (ipAddress.Octets[0] != cMulticastPrefix)
                {
                    throw std::invalid_argument(
                        "IPv6 multicast address must start with 0xFF.");
                }

                std::unique_ptr<Ipv6EndpointOption> _result(
                    new Ipv6EndpointOption(
                        OptionType::IPv6Multicast,
                        discardable,
                        ipAddress,
                        Layer4ProtocolType::Udp,
                        port));

                return _result;
            }

            std::unique_ptr<Ipv6EndpointOption> Ipv6EndpointOption::CreateSdEndpoint(
                bool discardable,
                helper::Ipv6Address ipAddress,
                Layer4ProtocolType protocol,
                uint16_t port) noexcept
            {
                std::unique_ptr<Ipv6EndpointOption> _result(
                    new Ipv6EndpointOption(
                        OptionType::IPv6SdEndpoint,
                        discardable,
                        ipAddress,
                        protocol,
                        port));

                return _result;
            }

            std::unique_ptr<Ipv6EndpointOption> Ipv6EndpointOption::Deserialize(
                const std::vector<uint8_t> &payload,
                std::size_t &offset,
                OptionType type,
                bool discardable)
            {
                helper::Ipv6Address _ipAddress =
                    helper::Ipv6Address::Extract(payload, offset);

                // Apply the reserved byte field offset
                offset++;

                auto _protocol =
                    static_cast<Layer4ProtocolType>(payload.at(offset++));

                uint16_t _port = helper::ExtractShort(payload, offset);

                switch (type)
                {
                case OptionType::IPv6Endpoint:
                    return CreateUnicastEndpoint(
                        discardable, _ipAddress, _protocol, _port);

                case OptionType::IPv6Multicast:
                    return CreateMulticastEndpoint(
                        discardable, _ipAddress, _port);

                case OptionType::IPv6SdEndpoint:
                    return CreateSdEndpoint(
                        discardable, _ipAddress, _protocol, _port);

                default:
                    throw std::out_of_range(
                        "The option type does not belong to IPv6 endpoint option series.");
                }
            }
        }
    }
}
