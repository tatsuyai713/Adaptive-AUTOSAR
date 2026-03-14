/// @file src/ara/per/file_storage.cpp
/// @brief Implementation for file storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./file_storage.h"
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstdio>

namespace ara
{
    namespace per
    {
        FileStorage::FileStorage(const std::string &basePath)
            : mBasePath{basePath}
        {
            // Ensure base directory exists
            ::mkdir(basePath.c_str(), 0755);
        }

        std::string FileStorage::GetFullPath(
            const std::string &fileName) const
        {
            return mBasePath + "/" + fileName;
        }

        core::Result<UniqueHandle<ReadAccessor>> FileStorage::OpenFileReadOnly(
            const std::string &fileName)
        {
            std::string fullPath = GetFullPath(fileName);
            struct stat st;
            if (::stat(fullPath.c_str(), &st) != 0)
            {
                return core::Result<UniqueHandle<ReadAccessor>>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            UniqueHandle<ReadAccessor> accessor(
                new ReadAccessor(fullPath));

            if (!accessor->IsValid())
            {
                return core::Result<UniqueHandle<ReadAccessor>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<UniqueHandle<ReadAccessor>>::FromValue(
                std::move(accessor));
        }

        core::Result<UniqueHandle<ReadWriteAccessor>>
        FileStorage::OpenFileReadWrite(const std::string &fileName)
        {
            std::string fullPath = GetFullPath(fileName);

            UniqueHandle<ReadWriteAccessor> accessor(
                new ReadWriteAccessor(fullPath));

            if (!accessor->IsValid())
            {
                return core::Result<UniqueHandle<ReadWriteAccessor>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<UniqueHandle<ReadWriteAccessor>>::FromValue(
                std::move(accessor));
        }

        core::Result<void> FileStorage::DeleteFile(
            const std::string &fileName)
        {
            std::string fullPath = GetFullPath(fileName);
            if (std::remove(fullPath.c_str()) != 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }
            return core::Result<void>::FromValue();
        }

        bool FileStorage::FileExists(const std::string &fileName) const
        {
            std::string fullPath = GetFullPath(fileName);
            struct stat st;
            return ::stat(fullPath.c_str(), &st) == 0;
        }

        core::Result<std::vector<std::string>>
        FileStorage::GetAllFileNames() const
        {
            std::vector<std::string> names;
            DIR *dir = ::opendir(mBasePath.c_str());
            if (!dir)
            {
                return core::Result<std::vector<std::string>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            struct dirent *entry;
            while ((entry = ::readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name != "." && name != "..")
                {
                    // Only include regular files
                    std::string fullPath = mBasePath + "/" + name;
                    struct stat st;
                    if (::stat(fullPath.c_str(), &st) == 0 &&
                        S_ISREG(st.st_mode))
                    {
                        names.push_back(name);
                    }
                }
            }
            ::closedir(dir);

            return core::Result<std::vector<std::string>>::FromValue(
                std::move(names));
        }

        core::Result<UniqueHandle<ReadWriteAccessor>>
        FileStorage::OpenFileWriteOnly(const std::string &fileName)
        {
            std::string fullPath = GetFullPath(fileName);

            // Truncate the file to provide write-only semantics
            UniqueHandle<ReadWriteAccessor> accessor(
                new ReadWriteAccessor(fullPath));

            if (!accessor->IsValid())
            {
                return core::Result<UniqueHandle<ReadWriteAccessor>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            // Truncate to zero length for write-only mode
            accessor->SetFileSize(0);

            return core::Result<UniqueHandle<ReadWriteAccessor>>::FromValue(
                std::move(accessor));
        }

        core::Result<void> FileStorage::RecoverFile(
            const std::string &fileName)
        {
            // Backup directory follows naming convention: basePath + ".bak"
            const std::string backupDir{mBasePath + ".bak"};
            const std::string backupPath{backupDir + "/" + fileName};
            const std::string targetPath{GetFullPath(fileName)};

            struct stat st;
            if (::stat(backupPath.c_str(), &st) != 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            // Read backup file
            std::ifstream source(backupPath, std::ios::binary);
            if (!source.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::ofstream target(targetPath, std::ios::binary | std::ios::trunc);
            if (!target.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            target << source.rdbuf();
            target.flush();

            if (!target.good())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> FileStorage::RecoverAllFiles()
        {
            const std::string backupDir{mBasePath + ".bak"};

            DIR *dir = ::opendir(backupDir.c_str());
            if (!dir)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            bool anyError{false};
            struct dirent *entry;
            while ((entry = ::readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name == "." || name == "..")
                {
                    continue;
                }

                std::string backupPath{backupDir + "/" + name};
                struct stat st;
                if (::stat(backupPath.c_str(), &st) == 0 &&
                    S_ISREG(st.st_mode))
                {
                    auto result = RecoverFile(name);
                    if (!result.HasValue())
                    {
                        anyError = true;
                    }
                }
            }
            ::closedir(dir);

            if (anyError)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<uint64_t> FileStorage::GetCurrentFileStorageSize() const
        {
            uint64_t totalSize{0};

            DIR *dir = ::opendir(mBasePath.c_str());
            if (!dir)
            {
                return core::Result<uint64_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            struct dirent *entry;
            while ((entry = ::readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name == "." || name == "..")
                {
                    continue;
                }

                std::string fullPath{mBasePath + "/" + name};
                struct stat st;
                if (::stat(fullPath.c_str(), &st) == 0 &&
                    S_ISREG(st.st_mode))
                {
                    totalSize += static_cast<uint64_t>(st.st_size);
                }
            }
            ::closedir(dir);

            return core::Result<uint64_t>::FromValue(totalSize);
        }

        core::Result<FileInfo> FileStorage::GetFileInfo(
            const std::string &fileName) const
        {
            std::string fullPath = GetFullPath(fileName);
            struct stat st;
            if (::stat(fullPath.c_str(), &st) != 0)
            {
                return core::Result<FileInfo>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            FileInfo info;
            info.fileName = fileName;
            info.size = static_cast<std::uint64_t>(st.st_size);
            info.creationTime = std::chrono::system_clock::from_time_t(st.st_ctime);
            info.modificationTime = std::chrono::system_clock::from_time_t(st.st_mtime);
            info.accessTime = std::chrono::system_clock::from_time_t(st.st_atime);
            info.isReadOnly = ((st.st_mode & S_IWUSR) == 0);

            return core::Result<FileInfo>::FromValue(std::move(info));
        }

        core::Result<uint64_t> FileStorage::GetCurrentFileStorageQuota() const
        {
            // In a full platform, quota is read from the ARXML manifest.
            // For this educational implementation, return 0 (no quota configured).
            return core::Result<uint64_t>::FromValue(0);
        }

        core::Result<uint64_t> FileStorage::EstimatedFreeSpace() const
        {
            auto quotaResult = GetCurrentFileStorageQuota();
            if (!quotaResult.HasValue())
            {
                return core::Result<uint64_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            uint64_t quota = quotaResult.Value();
            if (quota == 0U)
            {
                // No quota configured — use filesystem statvfs
                struct statvfs svfs;
                if (::statvfs(mBasePath.c_str(), &svfs) != 0)
                {
                    return core::Result<uint64_t>::FromError(
                        MakeErrorCode(PerErrc::kPhysicalStorageFailure));
                }
                return core::Result<uint64_t>::FromValue(
                    static_cast<uint64_t>(svfs.f_bavail) *
                    static_cast<uint64_t>(svfs.f_frsize));
            }

            auto sizeResult = GetCurrentFileStorageSize();
            if (!sizeResult.HasValue())
            {
                return core::Result<uint64_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            uint64_t used = sizeResult.Value();
            return core::Result<uint64_t>::FromValue(
                (used < quota) ? (quota - used) : 0U);
        }
    }
}
