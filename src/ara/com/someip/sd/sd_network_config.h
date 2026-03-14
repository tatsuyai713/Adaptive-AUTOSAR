/// @file src/ara/com/someip/sd/sd_network_config.h
/// @brief Network configuration for SOME/IP Service Discovery.
/// @details Per PRS_SOMEIPSD_00700, SD multicast messages should use
///          a configurable TTL (Time To Live) / Hop Limit for IPv4/IPv6.
///          This configuration also holds the multicast group address
///          and port used by SD endpoints.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SOMEIP_SD_NETWORK_CONFIG_H
#define ARA_COM_SOMEIP_SD_NETWORK_CONFIG_H

#include <cstdint>
#include <string>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                /// @brief SOME/IP-SD multicast network configuration.
                struct SdNetworkConfig
                {
                    /// @brief IPv4 multicast group address (default per AUTOSAR: 239.0.0.1)
                    std::string MulticastAddress{"239.0.0.1"};

                    /// @brief SD multicast port (default per AUTOSAR: 30490)
                    uint16_t MulticastPort{30490U};

                    /// @brief IP multicast TTL / Hop Limit.
                    ///        Per PRS_SOMEIPSD_00700, this should be configurable.
                    ///        Default 1 = link-local only.
                    uint8_t MulticastTtl{1U};

                    /// @brief Maximum number of entries per SD message
                    ///        (0 = unlimited, i.e. only limited by MTU).
                    uint16_t MaxEntriesPerMessage{0U};

                    /// @brief Enable reboot detection flag in SD messages
                    bool RebootDetection{true};
                };

                /// @brief Validate an SD network configuration.
                /// @param config Configuration to validate
                /// @returns true if the configuration is valid
                inline bool ValidateSdNetworkConfig(
                    const SdNetworkConfig &config) noexcept
                {
                    // TTL must be at least 1
                    if (config.MulticastTtl == 0U)
                    {
                        return false;
                    }

                    // Port must be non-zero
                    if (config.MulticastPort == 0U)
                    {
                        return false;
                    }

                    return true;
                }
            }
        }
    }
}

#endif
