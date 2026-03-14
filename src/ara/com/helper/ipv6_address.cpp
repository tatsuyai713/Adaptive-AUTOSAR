/// @file src/ara/com/helper/ipv6_address.cpp
/// @brief Implementation for ipv6 address.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "./ipv6_address.h"

namespace ara
{
    namespace com
    {
        namespace helper
        {
            Ipv6Address::Ipv6Address(
                uint8_t o0, uint8_t o1, uint8_t o2, uint8_t o3,
                uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7,
                uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11,
                uint8_t o12, uint8_t o13, uint8_t o14, uint8_t o15) noexcept
                : Octets{o0, o1, o2, o3, o4, o5, o6, o7,
                         o8, o9, o10, o11, o12, o13, o14, o15}
            {
            }

            Ipv6Address::Ipv6Address(
                const std::array<uint8_t, 16> &octets) noexcept
                : Octets{octets}
            {
            }

            Ipv6Address::Ipv6Address(const std::string &ipAddress)
            {
                // Parse colon-separated hex groups (full form: 8 groups of 4 hex digits)
                Octets.fill(0);
                std::string expanded;

                // Handle :: abbreviation by expanding to full form
                auto doubleColon = ipAddress.find("::");
                if (doubleColon != std::string::npos)
                {
                    std::string left = ipAddress.substr(0, doubleColon);
                    std::string right = ipAddress.substr(doubleColon + 2);

                    int leftGroups = 0;
                    int rightGroups = 0;
                    if (!left.empty())
                    {
                        leftGroups = 1;
                        for (char c : left)
                        {
                            if (c == ':')
                                ++leftGroups;
                        }
                    }
                    if (!right.empty())
                    {
                        rightGroups = 1;
                        for (char c : right)
                        {
                            if (c == ':')
                                ++rightGroups;
                        }
                    }

                    int missingGroups = 8 - leftGroups - rightGroups;
                    expanded = left;
                    for (int i = 0; i < missingGroups; ++i)
                    {
                        if (!expanded.empty())
                            expanded += ":";
                        expanded += "0";
                    }
                    if (!right.empty())
                    {
                        if (!expanded.empty())
                            expanded += ":";
                        expanded += right;
                    }
                }
                else
                {
                    expanded = ipAddress;
                }

                // Parse 8 colon-separated hex groups
                std::istringstream iss(expanded);
                for (int group = 0; group < 8; ++group)
                {
                    std::string token;
                    if (!std::getline(iss, token, ':'))
                    {
                        break;
                    }
                    unsigned int val = 0;
                    std::istringstream hexStream(token);
                    hexStream >> std::hex >> val;
                    Octets[group * 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
                    Octets[group * 2 + 1] = static_cast<uint8_t>(val & 0xFF);
                }
            }

            std::string Ipv6Address::ToString() const
            {
                std::ostringstream oss;
                for (int group = 0; group < 8; ++group)
                {
                    if (group > 0)
                    {
                        oss << ':';
                    }
                    uint16_t val = (static_cast<uint16_t>(Octets[group * 2]) << 8) |
                                   Octets[group * 2 + 1];
                    oss << std::hex << std::setfill('0') << std::setw(4) << val;
                }
                return oss.str();
            }

            void Ipv6Address::Inject(
                std::vector<uint8_t> &vector,
                const Ipv6Address &ipAddress)
            {
                vector.insert(
                    vector.end(),
                    ipAddress.Octets.begin(),
                    ipAddress.Octets.end());
            }

            Ipv6Address Ipv6Address::Extract(
                const std::vector<uint8_t> &vector,
                std::size_t &offset)
            {
                std::array<uint8_t, 16> octets;
                for (int i = 0; i < 16; ++i)
                {
                    octets[i] = vector.at(offset++);
                }
                return Ipv6Address(octets);
            }
        }
    }
}
