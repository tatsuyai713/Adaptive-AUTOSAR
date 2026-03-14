/// @file src/ara/com/instance_identifier.h
/// @brief InstanceIdentifier per AUTOSAR AP SWS_CM_00151.
/// @details An InstanceIdentifier is a concrete, transport-level identifier
///          that uniquely locates a service instance on the network/IPC.
///          It is distinct from InstanceSpecifier (which is a port-prototype
///          reference from the system model, resolved via the manifest).
///          InstanceIdentifier is the result of resolving an InstanceSpecifier.

#ifndef ARA_COM_INSTANCE_IDENTIFIER_H
#define ARA_COM_INSTANCE_IDENTIFIER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../core/result.h"
#include "../core/instance_specifier.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        /// @brief Concrete transport-level instance identifier per SWS_CM_00151.
        ///        Encapsulates the binding-specific addressing information
        ///        (e.g., SOME/IP service/instance IDs, DDS domain/topic, iceoryx channel).
        class InstanceIdentifier
        {
        private:
            std::string mValue;

        public:
            /// @brief Construct from a string representation.
            /// @param value String encoding of the transport-level identifier
            ///              (e.g., "someip:0x1234:0x0001" or "dds:VehicleTopic").
            explicit InstanceIdentifier(const std::string &value)
                : mValue{value}
            {
            }

            /// @brief Construct from SOME/IP-style numeric IDs.
            /// @param serviceId SOME/IP Service ID
            /// @param instanceId SOME/IP Instance ID
            InstanceIdentifier(std::uint16_t serviceId,
                               std::uint16_t instanceId)
                : mValue{"someip:" +
                         std::to_string(serviceId) + ":" +
                         std::to_string(instanceId)}
            {
            }

            /// @brief Get the string representation.
            /// @returns String encoding of the identifier.
            const std::string &ToString() const noexcept
            {
                return mValue;
            }

            /// @brief Equality comparison.
            bool operator==(const InstanceIdentifier &other) const noexcept
            {
                return mValue == other.mValue;
            }

            /// @brief Inequality comparison.
            bool operator!=(const InstanceIdentifier &other) const noexcept
            {
                return mValue != other.mValue;
            }

            /// @brief Less-than comparison for use in ordered containers.
            bool operator<(const InstanceIdentifier &other) const noexcept
            {
                return mValue < other.mValue;
            }

            /// @brief Resolve an InstanceSpecifier to concrete InstanceIdentifiers.
            /// @param specifier The InstanceSpecifier to resolve.
            /// @returns Container of resolved InstanceIdentifiers.
            static core::Result<std::vector<InstanceIdentifier>>
            Resolve(const core::InstanceSpecifier &specifier)
            {
                // In a full implementation, this would consult the manifest.
                // For this educational implementation, derive from the specifier string.
                std::vector<InstanceIdentifier> result;
                result.emplace_back(specifier.ToString());
                return core::Result<std::vector<InstanceIdentifier>>::FromValue(
                    std::move(result));
            }
        };

        /// @brief Container type for InstanceIdentifiers per SWS_CM_00152.
        using InstanceIdentifierContainer =
            std::vector<InstanceIdentifier>;
    }
}

namespace std
{
    /// @brief std::hash specialization for InstanceIdentifier.
    template <>
    struct hash<ara::com::InstanceIdentifier>
    {
        std::size_t operator()(
            const ara::com::InstanceIdentifier &id) const noexcept
        {
            return std::hash<std::string>{}(id.ToString());
        }
    };
}

#endif
