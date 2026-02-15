/// @file src/ara/per/file_storage.h
/// @brief Declarations for file storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <string>
#include <vector>
#include "../core/result.h"
#include "./per_error_domain.h"
#include "./read_accessor.h"
#include "./read_write_accessor.h"
#include "./shared_handle.h"

namespace ara
{
    namespace per
    {
        /// @brief File storage per AUTOSAR AP SWS_PER
        ///        Provides file-based persistent storage within a dedicated directory.
        class FileStorage
        {
        private:
            std::string mBasePath;

            std::string GetFullPath(const std::string &fileName) const;

        public:
            /// @brief Constructor
            /// @param basePath Directory path for file storage
            explicit FileStorage(const std::string &basePath);

            ~FileStorage() noexcept = default;

            FileStorage(const FileStorage &) = delete;
            FileStorage &operator=(const FileStorage &) = delete;

            /// @brief Open a file for reading
            /// @param fileName Name of the file (relative to storage directory)
            /// @returns ReadAccessor on success, error otherwise
            core::Result<UniqueHandle<ReadAccessor>> OpenFileReadOnly(
                const std::string &fileName);

            /// @brief Open a file for reading and writing
            /// @param fileName Name of the file (relative to storage directory)
            /// @returns ReadWriteAccessor on success, error otherwise
            core::Result<UniqueHandle<ReadWriteAccessor>> OpenFileReadWrite(
                const std::string &fileName);

            /// @brief Delete a file from storage
            /// @param fileName Name of the file to delete
            /// @returns Void Result on success
            core::Result<void> DeleteFile(const std::string &fileName);

            /// @brief Check if a file exists in storage
            /// @param fileName Name of the file
            /// @returns True if the file exists
            bool FileExists(const std::string &fileName) const;

            /// @brief Get all file names in storage
            /// @returns Vector of file names, or error
            core::Result<std::vector<std::string>> GetAllFileNames() const;
        };
    }
}

#endif
