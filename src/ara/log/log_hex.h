/// @file src/ara/log/log_hex.h
/// @brief LogHex and LogBin formatting wrappers for ara::log::LogStream.
/// @details Provides AUTOSAR AP SWS_LOG-compatible hex and binary formatting:
///          - LogHex(value)  — formats integral value as 0x<hexdigits>
///          - LogBin(value)  — formats integral value as 0b<binarydigits>
///          - LogHex(ptr, len) — formats a byte array as hex dump (space-separated)
///          - LogRaw(ptr, len) — formats a byte array as a raw hex dump (no spaces)
///
///          Usage:
///          @code
///          auto &logger = ara::log::CreateLogger("APP", "demo");
///          uint32_t val = 0xDEADBEEF;
///          logger.LogInfo() << "CRC=" << ara::log::LogHex(val);
///          // → CRC=0xDEADBEEF
///
///          uint8_t buf[] = {0x01, 0x02, 0x03};
///          logger.LogInfo() << "buf=" << ara::log::LogHex(buf, 3);
///          // → buf=01 02 03
///          @endcode
///
///          Reference: AUTOSAR SWS_LOG §8.3 (LogHex/LogBin)
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_LOG_LOG_HEX_H
#define ARA_LOG_LOG_HEX_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include "./log_stream.h"

namespace ara
{
    namespace log
    {
        // -----------------------------------------------------------------------
        // HexValue — wrapper returned by LogHex(integral)
        // -----------------------------------------------------------------------
        /// @brief Wraps an integral value for hex-formatted log output.
        template <typename T>
        class HexValue
        {
            static_assert(std::is_integral<T>::value,
                          "LogHex requires an integral type");
        public:
            explicit HexValue(T v) noexcept : mValue{v} {}

            std::string ToString() const
            {
                std::ostringstream oss;
                oss << "0x"
                    << std::uppercase
                    << std::hex
                    << std::setfill('0')
                    << std::setw(sizeof(T) * 2)
                    << static_cast<uintmax_t>(
                           static_cast<typename std::make_unsigned<T>::type>(mValue));
                return oss.str();
            }

        private:
            T mValue;
        };

        // -----------------------------------------------------------------------
        // HexBytes — wrapper returned by LogHex(ptr, len)
        // -----------------------------------------------------------------------
        /// @brief Wraps a byte array for space-separated hex dump log output.
        class HexBytes
        {
        public:
            HexBytes(const uint8_t *data, std::size_t len) noexcept
                : mData{data}, mLen{len} {}

            std::string ToString() const
            {
                std::ostringstream oss;
                for (std::size_t i = 0; i < mLen; ++i)
                {
                    if (i > 0) oss << ' ';
                    oss << std::uppercase << std::hex
                        << std::setfill('0') << std::setw(2)
                        << static_cast<unsigned>(mData[i]);
                }
                return oss.str();
            }

        private:
            const uint8_t *mData;
            std::size_t mLen;
        };

        // -----------------------------------------------------------------------
        // BinValue — wrapper returned by LogBin(integral)
        // -----------------------------------------------------------------------
        /// @brief Wraps an integral value for binary-formatted log output.
        template <typename T>
        class BinValue
        {
            static_assert(std::is_integral<T>::value,
                          "LogBin requires an integral type");
        public:
            explicit BinValue(T v) noexcept : mValue{v} {}

            std::string ToString() const
            {
                constexpr std::size_t bits = sizeof(T) * 8;
                using UT = typename std::make_unsigned<T>::type;
                const UT uv = static_cast<UT>(mValue);
                std::string result;
                result.reserve(2 + bits);
                result += "0b";
                for (std::size_t i = bits; i > 0; --i)
                {
                    result += ((uv >> (i - 1)) & 1U) ? '1' : '0';
                }
                return result;
            }

        private:
            T mValue;
        };

        // -----------------------------------------------------------------------
        // LogStream insertion operators for hex/bin wrappers
        // -----------------------------------------------------------------------
        template <typename T>
        inline LogStream &operator<<(LogStream &ls, const HexValue<T> &hv)
        {
            ls << hv.ToString();
            return ls;
        }

        inline LogStream &operator<<(LogStream &ls, const HexBytes &hb)
        {
            ls << hb.ToString();
            return ls;
        }

        template <typename T>
        inline LogStream &operator<<(LogStream &ls, const BinValue<T> &bv)
        {
            ls << bv.ToString();
            return ls;
        }

        // -----------------------------------------------------------------------
        // Factory functions (AUTOSAR SWS_LOG API)
        // -----------------------------------------------------------------------
        /// @brief Format an integral value as a hexadecimal string (0x...).
        /// @tparam T Integral type
        /// @param value Value to format
        /// @returns HexValue wrapper suitable for insertion into a LogStream
        template <typename T>
        inline HexValue<T> LogHex(T value) noexcept
        {
            return HexValue<T>{value};
        }

        /// @brief Format a byte array as a space-separated hex dump.
        /// @param data Pointer to byte array
        /// @param len  Number of bytes
        /// @returns HexBytes wrapper suitable for insertion into a LogStream
        inline HexBytes LogHex(const uint8_t *data, std::size_t len) noexcept
        {
            return HexBytes{data, len};
        }

        /// @brief Format an integral value as a binary string (0b...).
        /// @tparam T Integral type
        /// @param value Value to format
        /// @returns BinValue wrapper suitable for insertion into a LogStream
        template <typename T>
        inline BinValue<T> LogBin(T value) noexcept
        {
            return BinValue<T>{value};
        }

    } // namespace log
} // namespace ara

#endif // ARA_LOG_LOG_HEX_H
