/// @file src/ara/com/option/configuration_option.h
/// @brief DNS-SD Configuration Option (OptionType 0x01).
/// @details Per PRS_SOMEIPSD_00550, the Configuration option carries
///          DNS-SD-compatible key=value configuration strings for a
///          service instance.  The option payload is a sequence of
///          length-prefixed UTF-8 strings, each in the form "key=value".
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CONFIGURATION_OPTION_H
#define CONFIGURATION_OPTION_H

#include <memory>
#include <string>
#include <vector>
#include "./option.h"

namespace ara
{
    namespace com
    {
        namespace option
        {
            /// @brief SD Configuration Option (type 0x01)
            /// @details Carries DNS-SD TXT record style key=value pairs.
            ///          Each pair is stored as a length-prefixed UTF-8 string
            ///          in the option payload.
            class ConfigurationOption : public Option
            {
            private:
                /// @brief key=value strings (e.g. "protocol=someip")
                std::vector<std::string> mConfigurationStrings;
                bool mDiscardableFlag;

            public:
                ConfigurationOption() = delete;

                /// @brief Constructor
                /// @param discardable Indicates whether the option can be discarded
                /// @param configStrings DNS-SD key=value configuration strings
                ConfigurationOption(
                    bool discardable,
                    std::vector<std::string> configStrings) noexcept;

                virtual uint16_t Length() const noexcept override;

                /// @brief Get configuration strings
                /// @returns Reference to the key=value string list
                const std::vector<std::string> &ConfigurationStrings() const noexcept;

                virtual std::vector<uint8_t> Payload() const override;

                /// @brief Deserialize a configuration option from payload
                /// @param payload Serialized option payload byte array
                /// @param offset Deserializing offset in the payload
                /// @param discardable Indicates whether the option can be discarded
                /// @param optionLength Total option content length (from length field)
                /// @returns Deserialized configuration option
                static std::unique_ptr<ConfigurationOption> Deserialize(
                    const std::vector<uint8_t> &payload,
                    std::size_t &offset,
                    bool discardable,
                    uint16_t optionLength);
            };
        }
    }
}

#endif
