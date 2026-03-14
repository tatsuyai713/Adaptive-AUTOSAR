/// @file src/ara/com/helper/ipv6_address.h
/// @brief Declarations for ipv6 address.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef IPV6_ADDRESS_H
#define IPV6_ADDRESS_H

#include <array>
#include <vector>
#include <string>
#include <stdint.h>

namespace ara
{
    namespace com
    {
        namespace helper
        {
            /// @brief IPv6 address wrapper helper (128-bit / 16-octet)
            struct Ipv6Address
            {
                /// @brief IPv6 address octets (16 bytes)
                std::array<uint8_t, 16> Octets;

                Ipv6Address() = delete;

                /// @brief Constructor from 16 individual octets
                Ipv6Address(
                    uint8_t o0, uint8_t o1, uint8_t o2, uint8_t o3,
                    uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7,
                    uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11,
                    uint8_t o12, uint8_t o13, uint8_t o14, uint8_t o15) noexcept;

                /// @brief Constructor from raw octets array
                explicit Ipv6Address(
                    const std::array<uint8_t, 16> &octets) noexcept;

                /// @brief Constructor from colon-hex string (e.g., "fe80::1")
                explicit Ipv6Address(const std::string &ipAddress);

                /// @brief Convert the IP address to colon-hex string
                /// @return IP address in "xxxx:xxxx:..." format
                std::string ToString() const;

                ~Ipv6Address() noexcept = default;

                /// @brief Inject an IPv6 address into a byte vector
                /// @param vector Byte vector
                /// @param ipAddress IPv6 address to be injected
                static void Inject(
                    std::vector<uint8_t> &vector,
                    const Ipv6Address &ipAddress);

                /// @brief Extract an IPv6 address from a byte vector
                /// @param vector Byte vector
                /// @param offset Extract offset at the vector
                /// @returns Extracted IPv6 address
                static Ipv6Address Extract(
                    const std::vector<uint8_t> &vector,
                    std::size_t &offset);
            };

            /// @brief Ipv6Address equality operator override
            inline bool operator==(
                const Ipv6Address &address1,
                const Ipv6Address &address2)
            {
                return address1.Octets == address2.Octets;
            }

            /// @brief Ipv6Address inequality operator override
            inline bool operator!=(
                const Ipv6Address &address1,
                const Ipv6Address &address2)
            {
                return !(address1 == address2);
            }
        }
    }
}

#endif
