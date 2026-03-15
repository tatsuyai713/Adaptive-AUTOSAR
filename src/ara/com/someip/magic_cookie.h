/// @file src/ara/com/someip/magic_cookie.h
/// @brief SOME/IP Magic Cookie support for TCP stream resynchronization.
/// @details Per PRS_SOMEIP_00075, the Magic Cookie mechanism allows
///          detection of message boundaries in a TCP byte stream when
///          framing is lost.  A client sends a "Client Magic Cookie"
///          and a server sends a "Server Magic Cookie" — fixed-format
///          SOME/IP messages with well-known header values that can be
///          recognised in any position within the TCP stream.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SOMEIP_MAGIC_COOKIE_H
#define ARA_COM_SOMEIP_MAGIC_COOKIE_H

#include <cstdint>
#include <vector>
#include <cstring>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief Magic Cookie message type constant per PRS_SOMEIP_00075.
            static constexpr std::uint8_t cMagicCookieMessageType{0xFFU};

            /// @brief Client Magic Cookie fixed message ID.
            static constexpr std::uint32_t cClientMagicCookieMessageId{0xDEAD0000U};

            /// @brief Server Magic Cookie fixed message ID.
            static constexpr std::uint32_t cServerMagicCookieMessageId{0xDEAD8000U};

            /// @brief Client Magic Cookie fixed length field value.
            static constexpr std::uint32_t cClientMagicCookieLength{0x00000008U};

            /// @brief Server Magic Cookie fixed length field value.
            static constexpr std::uint32_t cServerMagicCookieLength{0x00000008U};

            /// @brief Fixed client/session IDs for magic cookies.
            static constexpr std::uint16_t cMagicCookieClientId{0xDEADU};
            static constexpr std::uint16_t cMagicCookieSessionId{0xDEADU};

            /// @brief Protocol / interface versions for magic cookies.
            static constexpr std::uint8_t cMagicCookieProtocolVersion{0x01U};
            static constexpr std::uint8_t cMagicCookieInterfaceVersion{0x01U};

            /// @brief Magic Cookie return code (always 0x00).
            static constexpr std::uint8_t cMagicCookieReturnCode{0x00U};

            /// @brief Generate the 16-byte Client Magic Cookie.
            /// @returns Serialized Client Magic Cookie message
            inline std::vector<std::uint8_t> GenerateClientMagicCookie() noexcept
            {
                return {
                    // Message ID (4 bytes)
                    0xDE, 0xAD, 0x00, 0x00,
                    // Length (4 bytes) = 8
                    0x00, 0x00, 0x00, 0x08,
                    // Client ID + Session ID
                    0xDE, 0xAD, 0xDE, 0xAD,
                    // Protocol Version, Interface Version, Message Type (0xFF), Return Code
                    cMagicCookieProtocolVersion,
                    cMagicCookieInterfaceVersion,
                    cMagicCookieMessageType,
                    cMagicCookieReturnCode};
            }

            /// @brief Generate the 16-byte Server Magic Cookie.
            /// @returns Serialized Server Magic Cookie message
            inline std::vector<std::uint8_t> GenerateServerMagicCookie() noexcept
            {
                return {
                    // Message ID (4 bytes)
                    0xDE, 0xAD, 0x80, 0x00,
                    // Length (4 bytes) = 8
                    0x00, 0x00, 0x00, 0x08,
                    // Client ID + Session ID
                    0xDE, 0xAD, 0xDE, 0xAD,
                    // Protocol Version, Interface Version, Message Type (0xFF), Return Code
                    cMagicCookieProtocolVersion,
                    cMagicCookieInterfaceVersion,
                    cMagicCookieMessageType,
                    cMagicCookieReturnCode};
            }

            /// @brief Check whether a 16-byte buffer contains a Client Magic Cookie.
            /// @param data Pointer to at least 16 bytes
            /// @param size Size of the buffer
            /// @returns true if the buffer starts with a valid client magic cookie
            inline bool IsClientMagicCookie(
                const std::uint8_t *data, std::size_t size) noexcept
            {
                if (size < 16U)
                {
                    return false;
                }
                const auto expected = GenerateClientMagicCookie();
                return std::memcmp(data, expected.data(), 16U) == 0;
            }

            /// @brief Check whether a 16-byte buffer contains a Server Magic Cookie.
            /// @param data Pointer to at least 16 bytes
            /// @param size Size of the buffer
            /// @returns true if the buffer starts with a valid server magic cookie
            inline bool IsServerMagicCookie(
                const std::uint8_t *data, std::size_t size) noexcept
            {
                if (size < 16U)
                {
                    return false;
                }
                const auto expected = GenerateServerMagicCookie();
                return std::memcmp(data, expected.data(), 16U) == 0;
            }

            /// @brief Check whether a buffer is any type of magic cookie.
            inline bool IsMagicCookie(
                const std::uint8_t *data, std::size_t size) noexcept
            {
                return IsClientMagicCookie(data, size) ||
                       IsServerMagicCookie(data, size);
            }

            /// @brief Scan a TCP byte stream for the next magic cookie boundary.
            /// @param data Pointer to the TCP stream buffer
            /// @param size Size of the buffer
            /// @returns Offset of the first magic cookie found, or size if none
            inline std::size_t FindMagicCookie(
                const std::uint8_t *data, std::size_t size) noexcept
            {
                if (size < 16U)
                {
                    return size;
                }

                for (std::size_t i = 0U; i <= size - 16U; ++i)
                {
                    if (IsMagicCookie(data + i, size - i))
                    {
                        return i;
                    }
                }
                return size;
            }
        }
    }
}

#endif
