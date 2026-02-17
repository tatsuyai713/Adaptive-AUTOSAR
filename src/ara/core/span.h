/// @file src/ara/core/span.h
/// @brief ara::core::Span — AUTOSAR AP SWS_CORE non-owning contiguous sequence view.
/// @details Provides ara::core::Span<T, Extent> as a C++14-compatible implementation
///          of a non-owning, contiguous-sequence view, conforming to AUTOSAR Adaptive
///          Platform Core specification (SWS_CORE_01900).
///
///          For C++20 environments, std::span is equivalent.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_SPAN_H
#define ARA_CORE_SPAN_H

#include <cstddef>
#include <iterator>
#include <array>
#include <vector>
#include <stdexcept>
#include <type_traits>

namespace ara
{
    namespace core
    {
        /// @brief Sentinel value for dynamic extent (size determined at runtime).
        static constexpr std::size_t dynamic_extent =
            static_cast<std::size_t>(-1);

        /// @brief Non-owning view of a contiguous sequence of objects (SWS_CORE_01900).
        /// @tparam T Element type (may be const-qualified for read-only spans)
        /// @tparam Extent Number of elements (static) or dynamic_extent (runtime)
        ///
        /// @details ara::core::Span provides a lightweight, non-owning reference to a
        ///          contiguous sequence of elements (array, vector, pointer+size, etc.).
        ///          No memory allocation or ownership semantics.
        ///
        /// @example
        /// @code
        /// std::vector<uint8_t> buf = {1, 2, 3, 4};
        /// ara::core::Span<uint8_t> view(buf.data(), buf.size());
        /// for (auto b : view) { process(b); }
        ///
        /// // Read-only span
        /// ara::core::Span<const uint8_t> ro_view(buf.data(), buf.size());
        /// @endcode
        template <typename T, std::size_t Extent = dynamic_extent>
        class Span
        {
        public:
            // -------------------------------------------------------------------
            // Member types
            // -------------------------------------------------------------------
            using element_type    = T;
            using value_type      = typename std::remove_cv<T>::type;
            using size_type       = std::size_t;
            using difference_type = std::ptrdiff_t;
            using pointer         = T *;
            using const_pointer   = const T *;
            using reference       = T &;
            using const_reference = const T &;
            using iterator        = T *;
            using const_iterator  = const T *;
            using reverse_iterator       = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;

            static constexpr size_type extent = Extent;

            // -------------------------------------------------------------------
            // Constructors
            // -------------------------------------------------------------------

            /// @brief Construct an empty span (only valid for Extent == 0 or dynamic_extent)
            constexpr Span() noexcept : mData{nullptr}, mSize{0}
            {
                static_assert(Extent == dynamic_extent || Extent == 0,
                              "Cannot default-construct a Span with non-zero static extent");
            }

            /// @brief Construct from pointer and count
            constexpr Span(pointer ptr, size_type count) noexcept
                : mData{ptr}, mSize{count} {}

            /// @brief Construct from pointer range [first, last)
            constexpr Span(pointer first, pointer last) noexcept
                : mData{first}, mSize{static_cast<size_type>(last - first)} {}

            /// @brief Construct from C-style array
            template <std::size_t N>
            constexpr Span(T (&arr)[N]) noexcept
                : mData{arr}, mSize{N} {}

            /// @brief Construct from std::array
            template <std::size_t N>
            constexpr Span(std::array<value_type, N> &arr) noexcept
                : mData{arr.data()}, mSize{N} {}

            /// @brief Construct from const std::array
            template <std::size_t N>
            constexpr Span(const std::array<value_type, N> &arr) noexcept
                : mData{arr.data()}, mSize{N} {}

            /// @brief Construct from std::vector
            Span(std::vector<value_type> &vec) noexcept
                : mData{vec.data()}, mSize{vec.size()} {}

            /// @brief Construct from const std::vector (read-only)
            Span(const std::vector<value_type> &vec) noexcept
                : mData{vec.data()}, mSize{vec.size()} {}

            /// @brief Copy constructor
            constexpr Span(const Span &) noexcept = default;

            /// @brief Copy assignment
            Span &operator=(const Span &) noexcept = default;

            // -------------------------------------------------------------------
            // Iterators
            // -------------------------------------------------------------------
            constexpr iterator begin() const noexcept { return mData; }
            constexpr iterator end() const noexcept { return mData + mSize; }
            constexpr const_iterator cbegin() const noexcept { return mData; }
            constexpr const_iterator cend() const noexcept { return mData + mSize; }
            reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
            reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

            // -------------------------------------------------------------------
            // Element access
            // -------------------------------------------------------------------
            /// @brief Access element by index (unchecked)
            constexpr reference operator[](size_type idx) const noexcept
            {
                return mData[idx];
            }

            /// @brief Access element by index with bounds check
            reference at(size_type idx) const
            {
                if (idx >= mSize)
                {
                    throw std::out_of_range("ara::core::Span::at: index out of range");
                }
                return mData[idx];
            }

            constexpr reference front() const noexcept { return mData[0]; }
            constexpr reference back() const noexcept { return mData[mSize - 1]; }
            constexpr pointer data() const noexcept { return mData; }

            // -------------------------------------------------------------------
            // Observers
            // -------------------------------------------------------------------
            constexpr size_type size() const noexcept { return mSize; }
            constexpr size_type size_bytes() const noexcept { return mSize * sizeof(T); }
            constexpr bool empty() const noexcept { return mSize == 0; }

            // -------------------------------------------------------------------
            // Subspans
            // -------------------------------------------------------------------

            /// @brief Return first N elements as a new Span
            Span<T> first(size_type count) const noexcept
            {
                return Span<T>(mData, count);
            }

            /// @brief Return last N elements as a new Span
            Span<T> last(size_type count) const noexcept
            {
                return Span<T>(mData + mSize - count, count);
            }

            /// @brief Return subspan starting at offset with given count
            Span<T> subspan(size_type offset,
                            size_type count = dynamic_extent) const noexcept
            {
                const size_type n = (count == dynamic_extent) ? (mSize - offset) : count;
                return Span<T>(mData + offset, n);
            }

        private:
            pointer   mData;
            size_type mSize;
        };

        // -----------------------------------------------------------------------
        // Convenience factory functions
        // -----------------------------------------------------------------------

        /// @brief Create a Span from a pointer and element count
        template <typename T>
        constexpr Span<T> MakeSpan(T *ptr, std::size_t count) noexcept
        {
            return Span<T>(ptr, count);
        }

        /// @brief Create a read-only Span from a const pointer and element count
        template <typename T>
        constexpr Span<const T> MakeSpan(const T *ptr, std::size_t count) noexcept
        {
            return Span<const T>(ptr, count);
        }

        /// @brief Create a Span from a C-style array
        template <typename T, std::size_t N>
        constexpr Span<T, N> MakeSpan(T (&arr)[N]) noexcept
        {
            return Span<T, N>(arr);
        }

        /// @brief Create a Span from a std::vector
        template <typename T>
        Span<T> MakeSpan(std::vector<T> &vec) noexcept
        {
            return Span<T>(vec.data(), vec.size());
        }

        /// @brief Create a read-only Span from a const std::vector
        template <typename T>
        Span<const T> MakeSpan(const std::vector<T> &vec) noexcept
        {
            return Span<const T>(vec.data(), vec.size());
        }

    } // namespace core
} // namespace ara

#endif // ARA_CORE_SPAN_H
