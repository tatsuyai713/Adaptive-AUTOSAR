/// @file src/ara/core/string.h
/// @brief ara::core::String — AUTOSAR AP SWS_CORE string type.
/// @details Provides ara::core::String as an alias for std::string, conforming to
///          the AUTOSAR Adaptive Platform Core specification (SWS_CORE_03000).
///          ara::core::StringView is provided as a non-owning read-only string view.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_STRING_H
#define ARA_CORE_STRING_H

#include <string>
#include <ostream>
#include <stdexcept>
#include <algorithm>

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP string type (SWS_CORE_03000).
        /// @details Alias for std::basic_string<char, std::char_traits<char>,
        ///          std::allocator<char>>. Provides all std::string operations.
        using String = std::string;

        /// @brief Non-owning read-only string view (SWS_CORE_03001).
        /// @details Alias for std::string_view (C++17). Provides a lightweight,
        ///          non-owning reference to a sequence of characters.
        /// @note Available only when compiled with C++17 or later.
        ///       For C++14 compatibility, use const String& or const char*.
#if __cplusplus >= 201703L
        using StringView = std::string_view;
#else
        /// @brief C++14-compatible lightweight string view (read-only reference wrapper).
        class StringView
        {
        public:
            using value_type = char;
            using size_type = std::size_t;
            static constexpr size_type npos = static_cast<size_type>(-1);

            constexpr StringView() noexcept : mData{nullptr}, mSize{0} {}

            constexpr StringView(const char *str, size_type len) noexcept
                : mData{str}, mSize{len} {}

            /// @brief Implicit conversion from const char* (null-terminated string)
            StringView(const char *str) noexcept
                : mData{str}, mSize{str ? std::char_traits<char>::length(str) : 0} {}

            /// @brief Implicit conversion from std::string
            StringView(const std::string &str) noexcept
                : mData{str.data()}, mSize{str.size()} {}

            constexpr const char *data() const noexcept { return mData; }
            constexpr size_type size() const noexcept { return mSize; }
            constexpr size_type length() const noexcept { return mSize; }
            constexpr bool empty() const noexcept { return mSize == 0; }

            constexpr const char *begin() const noexcept { return mData; }
            constexpr const char *end() const noexcept { return mData + mSize; }
            constexpr const char *cbegin() const noexcept { return mData; }
            constexpr const char *cend() const noexcept { return mData + mSize; }

            constexpr char operator[](size_type pos) const noexcept { return mData[pos]; }

            /// @brief Bounds-checked element access
            const char &at(size_type pos) const
            {
                if (pos >= mSize)
                    throw std::out_of_range("StringView::at");
                return mData[pos];
            }

            constexpr const char &front() const noexcept { return mData[0]; }
            constexpr const char &back() const noexcept { return mData[mSize - 1]; }

            /// @brief Shrink the view by moving the start forward
            void remove_prefix(size_type n) noexcept
            {
                mData += n;
                mSize -= n;
            }

            /// @brief Shrink the view by reducing the length
            void remove_suffix(size_type n) noexcept { mSize -= n; }

            void swap(StringView &other) noexcept
            {
                std::swap(mData, other.mData);
                std::swap(mSize, other.mSize);
            }

            /// @brief Copy characters to a buffer
            size_type copy(char *dest, size_type count, size_type pos = 0) const
            {
                if (pos > mSize)
                    throw std::out_of_range("StringView::copy");
                size_type rcount = std::min(count, mSize - pos);
                std::char_traits<char>::copy(dest, mData + pos, rcount);
                return rcount;
            }

            /// @brief Return a sub-view
            StringView substr(size_type pos = 0, size_type count = npos) const
            {
                if (pos > mSize)
                    throw std::out_of_range("StringView::substr");
                size_type rcount = std::min(count, mSize - pos);
                return StringView(mData + pos, rcount);
            }

            /// @brief Three-way lexicographic comparison
            int compare(StringView sv) const noexcept
            {
                size_type rlen = std::min(mSize, sv.mSize);
                int r = std::char_traits<char>::compare(mData, sv.mData, rlen);
                if (r == 0)
                {
                    if (mSize < sv.mSize) return -1;
                    if (mSize > sv.mSize) return 1;
                }
                return r;
            }

            int compare(size_type pos1, size_type count1, StringView sv) const
            {
                return substr(pos1, count1).compare(sv);
            }

            /// @brief Check prefix
            bool starts_with(StringView sv) const noexcept
            {
                return mSize >= sv.mSize &&
                       std::char_traits<char>::compare(mData, sv.mData, sv.mSize) == 0;
            }

            bool starts_with(char c) const noexcept
            {
                return !empty() && mData[0] == c;
            }

            /// @brief Check suffix
            bool ends_with(StringView sv) const noexcept
            {
                return mSize >= sv.mSize &&
                       std::char_traits<char>::compare(mData + mSize - sv.mSize,
                                                       sv.mData, sv.mSize) == 0;
            }

            bool ends_with(char c) const noexcept
            {
                return !empty() && mData[mSize - 1] == c;
            }

            /// @brief Find first occurrence of a substring
            size_type find(StringView sv, size_type pos = 0) const noexcept
            {
                if (sv.mSize == 0) return pos <= mSize ? pos : npos;
                if (pos + sv.mSize > mSize) return npos;
                for (size_type i = pos; i <= mSize - sv.mSize; ++i)
                {
                    if (std::char_traits<char>::compare(mData + i, sv.mData, sv.mSize) == 0)
                        return i;
                }
                return npos;
            }

            size_type find(char c, size_type pos = 0) const noexcept
            {
                for (size_type i = pos; i < mSize; ++i)
                    if (mData[i] == c) return i;
                return npos;
            }

            /// @brief Find last occurrence of a substring
            size_type rfind(StringView sv, size_type pos = npos) const noexcept
            {
                if (sv.mSize == 0) return std::min(pos, mSize);
                if (sv.mSize > mSize) return npos;
                size_type last = std::min(pos, mSize - sv.mSize);
                for (size_type i = last + 1; i > 0; --i)
                {
                    if (std::char_traits<char>::compare(mData + i - 1, sv.mData, sv.mSize) == 0)
                        return i - 1;
                }
                return npos;
            }

            size_type rfind(char c, size_type pos = npos) const noexcept
            {
                if (mSize == 0) return npos;
                size_type last = std::min(pos, mSize - 1);
                for (size_type i = last + 1; i > 0; --i)
                    if (mData[i - 1] == c) return i - 1;
                return npos;
            }

            /// @brief Find first character in set
            size_type find_first_of(StringView sv, size_type pos = 0) const noexcept
            {
                for (size_type i = pos; i < mSize; ++i)
                    for (size_type j = 0; j < sv.mSize; ++j)
                        if (mData[i] == sv.mData[j]) return i;
                return npos;
            }

            size_type find_first_of(char c, size_type pos = 0) const noexcept
            {
                return find(c, pos);
            }

            /// @brief Find last character in set
            size_type find_last_of(StringView sv, size_type pos = npos) const noexcept
            {
                if (mSize == 0) return npos;
                size_type last = std::min(pos, mSize - 1);
                for (size_type i = last + 1; i > 0; --i)
                    for (size_type j = 0; j < sv.mSize; ++j)
                        if (mData[i - 1] == sv.mData[j]) return i - 1;
                return npos;
            }

            size_type find_last_of(char c, size_type pos = npos) const noexcept
            {
                return rfind(c, pos);
            }

            /// @brief Find first character not in set
            size_type find_first_not_of(StringView sv, size_type pos = 0) const noexcept
            {
                for (size_type i = pos; i < mSize; ++i)
                {
                    bool found = false;
                    for (size_type j = 0; j < sv.mSize; ++j)
                        if (mData[i] == sv.mData[j]) { found = true; break; }
                    if (!found) return i;
                }
                return npos;
            }

            size_type find_first_not_of(char c, size_type pos = 0) const noexcept
            {
                for (size_type i = pos; i < mSize; ++i)
                    if (mData[i] != c) return i;
                return npos;
            }

            /// @brief Find last character not in set
            size_type find_last_not_of(StringView sv, size_type pos = npos) const noexcept
            {
                if (mSize == 0) return npos;
                size_type last = std::min(pos, mSize - 1);
                for (size_type i = last + 1; i > 0; --i)
                {
                    bool found = false;
                    for (size_type j = 0; j < sv.mSize; ++j)
                        if (mData[i - 1] == sv.mData[j]) { found = true; break; }
                    if (!found) return i - 1;
                }
                return npos;
            }

            size_type find_last_not_of(char c, size_type pos = npos) const noexcept
            {
                if (mSize == 0) return npos;
                size_type last = std::min(pos, mSize - 1);
                for (size_type i = last + 1; i > 0; --i)
                    if (mData[i - 1] != c) return i - 1;
                return npos;
            }

            /// @brief Convert to std::string (allocates memory)
            std::string to_string() const { return std::string(mData, mSize); }

            bool operator==(const StringView &other) const noexcept
            {
                return mSize == other.mSize &&
                       std::char_traits<char>::compare(mData, other.mData, mSize) == 0;
            }
            bool operator!=(const StringView &other) const noexcept
            {
                return !(*this == other);
            }

            bool operator<(const StringView &other) const noexcept { return compare(other) < 0; }
            bool operator>(const StringView &other) const noexcept { return compare(other) > 0; }
            bool operator<=(const StringView &other) const noexcept { return compare(other) <= 0; }
            bool operator>=(const StringView &other) const noexcept { return compare(other) >= 0; }

            friend std::ostream &operator<<(std::ostream &os, const StringView &sv)
            {
                os.write(sv.mData, static_cast<std::streamsize>(sv.mSize));
                return os;
            }

        private:
            const char *mData;
            size_type mSize;
        };
#endif // __cplusplus >= 201703L

    } // namespace core
} // namespace ara

#endif // ARA_CORE_STRING_H
