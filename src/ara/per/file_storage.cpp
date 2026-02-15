/// @file src/ara/per/file_storage.cpp
/// @brief Implementation for file storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./file_storage.h"
#include <dirent.h>
#include <sys/stat.h>
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
    }
}
