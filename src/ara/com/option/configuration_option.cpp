/// @file src/ara/com/option/configuration_option.cpp
/// @brief Implementation for configuration option.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./configuration_option.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            ConfigurationOption::ConfigurationOption(
                bool discardable,
                std::vector<std::string> configStrings) noexcept
                : Option(OptionType::Configuration, discardable),
                  mConfigurationStrings{std::move(configStrings)},
                  mDiscardableFlag{discardable}
            {
            }

            uint16_t ConfigurationOption::Length() const noexcept
            {
                // Each string is preceded by a 1-byte length prefix
                uint16_t totalLength = 0U;
                for (const auto &str : mConfigurationStrings)
                {
                    totalLength += static_cast<uint16_t>(1U + str.size());
                }
                return totalLength;
            }

            const std::vector<std::string> &
            ConfigurationOption::ConfigurationStrings() const noexcept
            {
                return mConfigurationStrings;
            }

            std::vector<uint8_t> ConfigurationOption::Payload() const
            {
                auto result = Option::BasePayload();

                for (const auto &str : mConfigurationStrings)
                {
                    auto len = static_cast<uint8_t>(
                        str.size() > 255U ? 255U : str.size());
                    result.push_back(len);
                    result.insert(result.end(), str.begin(),
                                  str.begin() + len);
                }

                return result;
            }

            std::unique_ptr<ConfigurationOption> ConfigurationOption::Deserialize(
                const std::vector<uint8_t> &payload,
                std::size_t &offset,
                bool discardable,
                uint16_t optionLength)
            {
                std::vector<std::string> configStrings;
                const std::size_t endOffset = offset + optionLength;

                while (offset < endOffset)
                {
                    const uint8_t strLen = payload.at(offset++);
                    if (strLen == 0U)
                    {
                        continue;
                    }
                    if (offset + strLen > endOffset)
                    {
                        break;
                    }
                    std::string str(
                        payload.begin() + offset,
                        payload.begin() + offset + strLen);
                    offset += strLen;
                    configStrings.push_back(std::move(str));
                }

                return std::make_unique<ConfigurationOption>(
                    discardable, std::move(configStrings));
            }
        }
    }
}
