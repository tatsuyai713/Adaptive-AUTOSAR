/// @file src/ara/per/read_accessor.h
/// @brief Declarations for read accessor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef READ_ACCESSOR_H
#define READ_ACCESSOR_H

#include <cstdint>
#include <fstream>
#include <string>
#include "../core/result.h"
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        /// @brief Seek origin for file position operations
        enum class SeekOrigin
        {
            kBeginning = 0,
            kCurrent = 1,
            kEnd = 2
        };

        /// @brief Read-only accessor for file storage per AUTOSAR AP SWS_PER
        class ReadAccessor
        {
        protected:
            std::ifstream mStream;
            std::string mFilePath;

        public:
            /// @brief Constructor
            /// @param filePath Path to the file to open for reading
            explicit ReadAccessor(const std::string &filePath);

            virtual ~ReadAccessor() noexcept;

            ReadAccessor(const ReadAccessor &) = delete;
            ReadAccessor &operator=(const ReadAccessor &) = delete;
            ReadAccessor(ReadAccessor &&other) noexcept;
            ReadAccessor &operator=(ReadAccessor &&other) noexcept;

            /// @brief Read data from the file
            /// @param buffer Destination buffer
            /// @param count Maximum number of bytes to read
            /// @returns Number of bytes actually read, or error
            core::Result<std::size_t> Read(
                std::uint8_t *buffer, std::size_t count);

            /// @brief Get the total file size
            /// @returns File size in bytes, or error
            core::Result<std::uint64_t> GetSize() const;

            /// @brief Peek at the next byte without consuming it
            /// @param byte Output byte
            /// @returns Void Result on success, error if at end of file
            core::Result<void> Peek(std::uint8_t &byte);

            /// @brief Seek to a position in the file
            /// @param offset Byte offset relative to origin
            /// @param origin Reference point for offset
            /// @returns Void Result on success, error if seek fails
            core::Result<void> Seek(
                std::int64_t offset,
                SeekOrigin origin = SeekOrigin::kBeginning);

            /// @brief Get the current read position
            /// @returns Current position in bytes, or error
            core::Result<std::uint64_t> GetCurrentPosition();

            /// @brief Check if the stream is in a good state
            bool IsValid() const noexcept;
        };
    }
}

#endif
