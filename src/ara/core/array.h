/// @file src/ara/core/array.h
/// @brief ara::core::Array — AUTOSAR AP SWS_CORE fixed-size array type.
/// @details Provides ara::core::Array<T, N> as an alias for std::array<T, N>,
///          conforming to AUTOSAR Adaptive Platform Core specification (SWS_CORE_11300).
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_ARRAY_H
#define ARA_CORE_ARRAY_H

#include <array>

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP fixed-size array type (SWS_CORE_11300).
        /// @tparam T Element type
        /// @tparam N Number of elements (compile-time constant)
        /// @details Alias for std::array<T, N>. Provides all std::array operations
        ///          including fill(), swap(), size(), begin(), end(), and operator[].
        ///          Unlike std::vector, no dynamic allocation occurs.
        ///
        /// @example
        /// @code
        /// ara::core::Array<uint8_t, 8> can_frame{};
        /// can_frame.fill(0x00);
        /// can_frame[0] = 0xFF;
        ///
        /// // Structured binding (C++17)
        /// auto [a, b, c] = ara::core::Array<int, 3>{1, 2, 3};
        /// @endcode
        template <typename T, std::size_t N>
        using Array = std::array<T, N>;

    } // namespace core
} // namespace ara

#endif // ARA_CORE_ARRAY_H
