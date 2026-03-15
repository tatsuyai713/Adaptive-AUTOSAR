/// @file src/ara/com/option/option_deserializer.cpp
/// @brief Implementation for option deserializer.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./option_deserializer.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            std::unique_ptr<Option> OptionDeserializer::Deserialize(
                const std::vector<uint8_t> &payload,
                std::size_t &offset)
            {
                // Read the option length field
                uint16_t _optionLength = helper::ExtractShort(payload, offset);

                auto _type = static_cast<OptionType>(payload.at(offset++));
                auto _discardable = static_cast<bool>(payload.at(offset++));

                switch (_type)
                {
                case OptionType::Configuration:
                    return ConfigurationOption::Deserialize(
                        payload, offset, _discardable, _optionLength);

                case OptionType::IPv4Endpoint:
                case OptionType::IPv4Multicast:
                case OptionType::IPv4SdEndpoint:
                    return Ipv4EndpointOption::Deserialize(
                        payload, offset, _type, _discardable);

                case OptionType::IPv6Endpoint:
                case OptionType::IPv6Multicast:
                case OptionType::IPv6SdEndpoint:
                    return Ipv6EndpointOption::Deserialize(
                        payload, offset, _type, _discardable);

                case OptionType::LoadBalancing:
                    return LoadBalancingOption::Deserialize(
                        payload, offset, _discardable);

                default:
                    throw std::out_of_range(
                        "Option type is not supported for deserializing.");
                }
            }
        }
    }
}