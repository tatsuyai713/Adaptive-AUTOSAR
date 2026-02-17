/// @file src/ara/core/map.h
/// @brief ara::core::Map — AUTOSAR AP SWS_CORE ordered map type.
/// @details Provides ara::core::Map<Key, T, Compare, Allocator> as an alias for
///          std::map, conforming to AUTOSAR Adaptive Platform Core specification
///          (SWS_CORE_11400 — ordered associative container).
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_MAP_H
#define ARA_CORE_MAP_H

#include <map>
#include <unordered_map>

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP ordered key-value map type (SWS_CORE_11400).
        /// @tparam Key Key type
        /// @tparam T Mapped value type
        /// @tparam Compare Comparator (defaults to std::less<Key>)
        /// @tparam Allocator Allocator (defaults to std::allocator<std::pair<const Key, T>>)
        /// @details Alias for std::map<Key, T, Compare, Allocator>.
        ///          Provides ordered iteration, O(log n) lookup, insert, and erase.
        ///
        /// @example
        /// @code
        /// ara::core::Map<ara::core::String, int> scores;
        /// scores["ECU_A"] = 100;
        /// auto it = scores.find("ECU_A");
        /// @endcode
        template <
            typename Key,
            typename T,
            typename Compare = std::less<Key>,
            typename Allocator = std::allocator<std::pair<const Key, T>>>
        using Map = std::map<Key, T, Compare, Allocator>;

        /// @brief AUTOSAR AP unordered key-value map type.
        /// @tparam Key Key type (must be hashable)
        /// @tparam T Mapped value type
        /// @tparam Hash Hash function (defaults to std::hash<Key>)
        /// @tparam KeyEqual Equality predicate
        /// @tparam Allocator Allocator
        /// @details Alias for std::unordered_map. O(1) average lookup, insert, and erase.
        ///          Use when iteration order is not required and Key is hashable.
        ///
        /// @note AUTOSAR SWS_CORE does not define a separate unordered map type,
        ///       but it is provided here for completeness and performance-sensitive use cases.
        template <
            typename Key,
            typename T,
            typename Hash = std::hash<Key>,
            typename KeyEqual = std::equal_to<Key>,
            typename Allocator = std::allocator<std::pair<const Key, T>>>
        using UnorderedMap = std::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

    } // namespace core
} // namespace ara

#endif // ARA_CORE_MAP_H
