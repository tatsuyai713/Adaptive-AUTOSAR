/// @file src/ara/sm/network_interface_controller.h
/// @brief Per-interface OS-level network control for SM.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SM_NETWORK_INTERFACE_CONTROLLER_H
#define SM_NETWORK_INTERFACE_CONTROLLER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Link status of a network interface.
        enum class LinkStatus : uint8_t
        {
            kDown = 0,
            kUp = 1,
            kUnknown = 2
        };

        /// @brief Snapshot of a single network interface.
        struct InterfaceInfo
        {
            std::string Name;
            LinkStatus Status{LinkStatus::kUnknown};
            bool AdminEnabled{false};
            std::string Ipv4Address;
            uint32_t Mtu{1500};
        };

        /// @brief Controls OS-level network interfaces (enable/disable/query).
        class NetworkInterfaceController
        {
        public:
            NetworkInterfaceController() = default;
            ~NetworkInterfaceController() = default;

            /// @brief Register a known interface by name.
            core::Result<void> RegisterInterface(const std::string &name);

            /// @brief Enable (bring up) an interface.
            core::Result<void> EnableInterface(const std::string &name);

            /// @brief Disable (bring down) an interface.
            core::Result<void> DisableInterface(const std::string &name);

            /// @brief Set MTU on an interface.
            core::Result<void> SetMtu(
                const std::string &name, uint32_t mtu);

            /// @brief Set IPv4 address on an interface.
            core::Result<void> SetIpv4Address(
                const std::string &name,
                const std::string &address);

            /// @brief Get info for a single interface.
            core::Result<InterfaceInfo> GetInterfaceInfo(
                const std::string &name) const;

            /// @brief Get info for all registered interfaces.
            std::vector<InterfaceInfo> GetAllInterfaces() const;

            /// @brief Check whether an interface is enabled.
            core::Result<bool> IsEnabled(const std::string &name) const;

        private:
            mutable std::mutex mMutex;
            std::map<std::string, InterfaceInfo> mInterfaces;
        };
    }
}

#endif
