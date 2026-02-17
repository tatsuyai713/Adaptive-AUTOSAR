/// @file src/ara/log/log_stream.h
/// @brief Declarations for log stream.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef LOG_STREAM_H
#define LOG_STREAM_H

#include <cstdint>
#include <vector>
#include <utility>
#include "../core/error_code.h"
#include "../core/instance_specifier.h"
#include "./common.h"
#include "./argument.h"

namespace ara
{
    namespace log
    {
        /// @brief A stream pipeline to combine log entities
        class LogStream final
        {
        private:
            std::string mLogs;
            void concat(std::string &&log);

        public:
            /// @brief Clear the stream
            void Flush() noexcept;

            /// @brief Arugment insertion operator
            /// @tparam T Argument playload type
            /// @param arg An agrgument
            /// @returns Reference to the current log stream
            template <typename T>
            LogStream &operator<<(const Argument<T> &arg)
            {
                std::string _argumentString = arg.ToString();
                concat(std::move(_argumentString));

                return *this;
            }

            /// @brief LogStream insertion operator
            /// @param value Another logstream
            /// @returns Reference to the current log stream
            LogStream &operator<<(const LogStream &value);

            /// @brief Boolean insertion operator
            /// @param value A boolean value
            /// @returns Reference to the current log stream
            LogStream &operator<<(bool value);

            /// @brief Signed 8-bit integer insertion operator
            LogStream &operator<<(int8_t value);

            /// @brief Unsigned 8-bit integer insertion operator
            LogStream &operator<<(uint8_t value);

            /// @brief Signed 16-bit integer insertion operator
            LogStream &operator<<(int16_t value);

            /// @brief Unsigned 16-bit integer insertion operator
            LogStream &operator<<(uint16_t value);

            /// @brief Signed 32-bit integer insertion operator
            LogStream &operator<<(int32_t value);

            /// @brief Unsigned 32-bit integer insertion operator
            LogStream &operator<<(uint32_t value);

            /// @brief Signed 64-bit integer insertion operator
            LogStream &operator<<(int64_t value);

            /// @brief Unsigned 64-bit integer insertion operator
            LogStream &operator<<(uint64_t value);

            /// @brief Float insertion operator
            /// @param value A float value
            /// @returns Reference to the current log stream
            LogStream &operator<<(float value);

            /// @brief Double-precision float insertion operator
            /// @param value A double value
            /// @returns Reference to the current log stream
            LogStream &operator<<(double value);

            /// @brief String insertion operator
            /// @param value A string
            /// @returns Reference to the current log stream
            LogStream &operator<<(const std::string &value);

            /// @brief C-syle string insertion operator
            /// @param value Character array
            /// @returns Reference to the current log stream
            LogStream &operator<<(const char *value);

            /// @brief LogLeve insertion operator
            /// @param value Log severity level
            /// @returns Reference to the current log stream
            LogStream &operator<<(LogLevel value);

            /// @brief ErrorCode insertion operator
            /// @param value An error code object
            /// @returns Reference to the current log stream
            LogStream &operator<<(const ara::core::ErrorCode &value);

            /// @brief InstanceSpecifier insertion operator
            /// @param value An instance specifier object
            /// @returns Reference to the current log stream
            LogStream &operator<<(const ara::core::InstanceSpecifier &value) noexcept;

            /// @brief Data array insertion operator
            /// @param value Data byte vector
            /// @returns Reference to the current log stream
            LogStream &operator<<(std::vector<std::uint8_t> value);

            /// @brief Log stream at a certian file and a certian line within the file
            /// @param file File name
            /// @param line Line number
            /// @returns Reference to the current log stream
            LogStream &WithLocation(std::string file, int line);

            /// @brief Convert the current log stream to a standard string
            /// @returns Serialized log stream string
            std::string ToString() const noexcept;
        };
    }
}

#endif