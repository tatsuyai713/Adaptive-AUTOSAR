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
