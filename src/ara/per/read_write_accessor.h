/// @file src/ara/per/read_write_accessor.h
/// @brief Declarations for read write accessor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef READ_WRITE_ACCESSOR_H
#define READ_WRITE_ACCESSOR_H

#include <cstdint>
#include <fstream>
#include <string>
#include "../core/result.h"
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        /// @brief Read-write accessor for file storage per AUTOSAR AP SWS_PER
        class ReadWriteAccessor
        {
        private:
            std::fstream mStream;
            std::string mFilePath;

        public:
            /// @brief Constructor
            /// @param filePath Path to the file to open for reading and writing
            explicit ReadWriteAccessor(const std::string &filePath);

            ~ReadWriteAccessor() noexcept;

            ReadWriteAccessor(const ReadWriteAccessor &) = delete;
            ReadWriteAccessor &operator=(const ReadWriteAccessor &) = delete;
            ReadWriteAccessor(ReadWriteAccessor &&other) noexcept;
            ReadWriteAccessor &operator=(ReadWriteAccessor &&other) noexcept;

            /// @brief Read data from the file
            /// @param buffer Destination buffer
            /// @param count Maximum number of bytes to read
            /// @returns Number of bytes actually read, or error
            core::Result<std::size_t> Read(
                std::uint8_t *buffer, std::size_t count);

            /// @brief Write data to the file
            /// @param data Source buffer
            /// @param count Number of bytes to write
            /// @returns Number of bytes actually written, or error
            core::Result<std::size_t> Write(
                const std::uint8_t *data, std::size_t count);

            /// @brief Flush file buffers to storage
            /// @returns Void Result on success
            core::Result<void> Sync();

            /// @brief Get the total file size
            /// @returns File size in bytes, or error
            core::Result<std::uint64_t> GetSize() const;

            /// @brief Truncate or extend the file to a given size
            /// @param size Desired file size
            /// @returns Void Result on success
            core::Result<void> SetFileSize(std::uint64_t size);

            /// @brief Check if the stream is in a good state
            bool IsValid() const noexcept;
        };
    }
}

#endif
