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

            /// @brief Check if the stream is in a good state
            bool IsValid() const noexcept;
        };
    }
}

#endif
