/// @file src/ara/per/file_storage.h
/// @brief Declarations for file storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "../core/result.h"
#include "./per_error_domain.h"
#include "./read_accessor.h"
#include "./read_write_accessor.h"
#include "./shared_handle.h"

namespace ara
{
    namespace per
    {
        /// @brief File metadata information (SWS_PER_00410).
        struct FileInfo
        {
            std::string fileName;                                    ///< File name (relative)
            std::uint64_t size{0};                                   ///< File size in bytes
            std::chrono::system_clock::time_point creationTime;      ///< Creation time
            std::chrono::system_clock::time_point modificationTime;  ///< Last modification time
            std::chrono::system_clock::time_point accessTime;        ///< Last access time
            bool isReadOnly{false};                                  ///< Read-only flag
        };

        /// @brief File storage per AUTOSAR AP SWS_PER
        ///        Provides file-based persistent storage within a dedicated directory.
        class FileStorage
        {
        private:
            std::string mBasePath;
            /// @brief Storage quota in bytes (0 = unlimited).
            std::uint64_t mQuotaBytes{0U};

            std::string GetFullPath(const std::string &fileName) const;

        public:
            /// @brief Constructor
            /// @param basePath   Directory path for file storage.
            /// @param quotaBytes Maximum total storage size in bytes (0 = unlimited).
            explicit FileStorage(const std::string &basePath,
                                 std::uint64_t quotaBytes = 0U);

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

            /// @brief Open a file for writing only (SWS_PER_00144)
            /// @param fileName Name of the file (relative to storage directory)
            /// @returns ReadWriteAccessor (write-only mode) on success, error otherwise
            core::Result<UniqueHandle<ReadWriteAccessor>> OpenFileWriteOnly(
                const std::string &fileName);

            /// @brief Get all file names in storage
            /// @returns Vector of file names, or error
            core::Result<std::vector<std::string>> GetAllFileNames() const;

            /// @brief Recover a single file from a backup (SWS_PER_00337)
            /// @param fileName Name of the file to recover
            /// @returns Void Result on success
            core::Result<void> RecoverFile(const std::string &fileName);

            /// @brief Recover all files inside this storage (SWS_PER_00419)
            /// @returns Void Result on success
            core::Result<void> RecoverAllFiles();

            /// @brief Get the total size consumed by all files in this storage (SWS_PER_00409)
            /// @returns Total size in bytes, or error
            core::Result<uint64_t> GetCurrentFileStorageSize() const;

            /// @brief Get metadata for a specific file (SWS_PER_00410).
            /// @param fileName Name of the file (relative to storage directory)
            /// @returns FileInfo on success, error otherwise
            core::Result<FileInfo> GetFileInfo(const std::string &fileName) const;

            /// @brief Get the quota (maximum allowed size) for this storage (SWS_PER_00408).
            /// @returns Maximum size in bytes, or error. Returns 0 if no quota is configured.
            core::Result<uint64_t> GetCurrentFileStorageQuota() const;

            /// @brief Estimate available free space in this storage (SWS_PER_00450).
            /// @returns Estimated free bytes (quota - currentSize), or error.
            core::Result<uint64_t> EstimatedFreeSpace() const;
        };
    }
}

#endif
