/// @file src/ara/core/byte.h
/// @brief ara::core::Byte — AUTOSAR Adaptive Platform standard byte type.
/// @details Provides a type-safe byte representation per SWS_CORE_10100.
///          This is equivalent to std::byte in C++17.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_BYTE_H
#define ARA_CORE_BYTE_H

#include <cstdint>
#include <type_traits>

namespace ara
{
    namespace core
    {
        /// @brief A type-safe byte representation (SWS_CORE_10100).
        /// @details Mirrors std::byte semantics for C++14 compatibility.
        enum class Byte : std::uint8_t {};

        // --- Bitwise operators (SWS_CORE_10101-10106) ---

        /// @brief Bitwise OR
        constexpr Byte operator|(Byte lhs, Byte rhs) noexcept
        {
            return static_cast<Byte>(
                static_cast<std::uint8_t>(lhs) |
                static_cast<std::uint8_t>(rhs));
        }

        /// @brief Bitwise AND
        constexpr Byte operator&(Byte lhs, Byte rhs) noexcept
        {
            return static_cast<Byte>(
                static_cast<std::uint8_t>(lhs) &
                static_cast<std::uint8_t>(rhs));
        }

        /// @brief Bitwise XOR
        constexpr Byte operator^(Byte lhs, Byte rhs) noexcept
        {
            return static_cast<Byte>(
                static_cast<std::uint8_t>(lhs) ^
                static_cast<std::uint8_t>(rhs));
        }

        /// @brief Bitwise NOT
        constexpr Byte operator~(Byte b) noexcept
        {
            return static_cast<Byte>(
                ~static_cast<std::uint8_t>(b));
        }

        /// @brief Bitwise OR-assign
        inline Byte &operator|=(Byte &lhs, Byte rhs) noexcept
        {
            lhs = lhs | rhs;
            return lhs;
        }

        /// @brief Bitwise AND-assign
        inline Byte &operator&=(Byte &lhs, Byte rhs) noexcept
        {
            lhs = lhs & rhs;
            return lhs;
        }

        /// @brief Bitwise XOR-assign
        inline Byte &operator^=(Byte &lhs, Byte rhs) noexcept
        {
            lhs = lhs ^ rhs;
            return lhs;
        }

        // --- Shift operators ---

        /// @brief Left shift
        template <typename IntegerType,
                  typename = typename std::enable_if<std::is_integral<IntegerType>::value>::type>
        constexpr Byte operator<<(Byte b, IntegerType shift) noexcept
        {
            return static_cast<Byte>(
                static_cast<std::uint8_t>(b) << shift);
        }

        /// @brief Right shift
        template <typename IntegerType,
                  typename = typename std::enable_if<std::is_integral<IntegerType>::value>::type>
        constexpr Byte operator>>(Byte b, IntegerType shift) noexcept
        {
            return static_cast<Byte>(
                static_cast<std::uint8_t>(b) >> shift);
        }

        /// @brief Left shift-assign
        template <typename IntegerType,
                  typename = typename std::enable_if<std::is_integral<IntegerType>::value>::type>
        inline Byte &operator<<=(Byte &b, IntegerType shift) noexcept
        {
            b = b << shift;
            return b;
        }

        /// @brief Right shift-assign
        template <typename IntegerType,
                  typename = typename std::enable_if<std::is_integral<IntegerType>::value>::type>
        inline Byte &operator>>=(Byte &b, IntegerType shift) noexcept
        {
            b = b >> shift;
            return b;
        }

        /// @brief Convert byte to integer type (SWS_CORE_10110).
        template <typename IntegerType,
                  typename = typename std::enable_if<std::is_integral<IntegerType>::value>::type>
        constexpr IntegerType to_integer(Byte b) noexcept
        {
            return static_cast<IntegerType>(b);
        }

    } // namespace core
} // namespace ara

#endif // ARA_CORE_BYTE_H
