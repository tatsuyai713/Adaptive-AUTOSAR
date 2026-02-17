/// @file src/ara/core/vector.h
/// @brief ara::core::Vector — AUTOSAR AP SWS_CORE dynamic array type.
/// @details Provides ara::core::Vector<T, Allocator> as an alias for std::vector,
///          conforming to AUTOSAR Adaptive Platform Core specification (SWS_CORE_11200).
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_VECTOR_H
#define ARA_CORE_VECTOR_H

#include <vector>

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP dynamic array type (SWS_CORE_11200).
        /// @tparam T Element type
        /// @tparam Allocator Allocator type (defaults to std::allocator<T>)
        /// @details Alias for std::vector<T, Allocator>. Provides all std::vector
        ///          operations (push_back, emplace_back, size, begin, end, etc.).
        ///
        /// @example
        /// @code
        /// ara::core::Vector<int> vec{1, 2, 3};
        /// vec.push_back(4);
        /// for (auto& v : vec) { /* ... */ }
        /// @endcode
        template <typename T, typename Allocator = std::allocator<T>>
        using Vector = std::vector<T, Allocator>;

    } // namespace core
} // namespace ara

#endif // ARA_CORE_VECTOR_H
