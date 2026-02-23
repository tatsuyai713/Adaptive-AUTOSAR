/// @file src/ara/per/persistency.cpp
/// @brief Implementation for persistency.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./persistency.h"
#include "../core/ap_release_info.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <limits>
#include <sys/stat.h>

namespace ara
{
    namespace per
    {
        namespace
        {
            constexpr std::uint16_t cPersistencySchemaVersion{
                ara::core::ApReleaseInfo::cReleaseCompact};

            const std::string cStorageRoot{"/tmp/ara_per"};
            const std::string cKeyValueStorageFileName{"kvs.dat"};
            const std::string cKeyValueStorageBackupFileName{"kvs.dat.bak"};
            const std::string cFileStorageDirName{"files"};
            const std::string cFileStorageBackupDirName{"files.bak"};
            const std::string cSchemaVersionFileName{"schema.version"};
            const std::string cSchemaVersionKey{"__ara_per_schema_version"};
            const std::string cReleaseProfileKey{"__ara_ap_release_profile"};

            /// @brief Check whether a filesystem path exists.
            bool PathExists(const std::string &path)
            {
                struct stat st;
                return (::stat(path.c_str(), &st) == 0);
            }

            /// @brief Check whether a path points to a directory.
            bool IsDirectory(const std::string &path)
            {
                struct stat st;
                return (::stat(path.c_str(), &st) == 0) &&
                       S_ISDIR(st.st_mode);
            }

            /// @brief Ensure that a directory exists.
            bool EnsureDirectoryExists(const std::string &path)
            {
                if (IsDirectory(path))
                {
                    return true;
                }

                if (::mkdir(path.c_str(), 0755) == 0)
                {
                    return true;
                }

                return (errno == EEXIST) && IsDirectory(path);
            }

            /// @brief Remove file if it exists.
            bool RemoveFileIfExists(const std::string &filePath)
            {
                if (::remove(filePath.c_str()) == 0)
                {
                    return true;
                }

                return errno == ENOENT;
            }

            /// @brief Copy one file atomically.
            bool CopyFile(const std::string &sourcePath, const std::string &targetPath)
            {
                std::ifstream source(sourcePath, std::ios::binary);
                if (!source.is_open())
                {
                    return false;
                }

                const std::string tmpPath{targetPath + ".tmp"};
                std::ofstream target(tmpPath, std::ios::binary | std::ios::trunc);
                if (!target.is_open())
                {
                    return false;
                }

                target << source.rdbuf();
                target.flush();

                if (!target.good() || (!source.good() && !source.eof()))
                {
                    target.close();
                    ::remove(tmpPath.c_str());
                    return false;
                }

                target.close();

                if (::rename(tmpPath.c_str(), targetPath.c_str()) != 0)
                {
                    ::remove(tmpPath.c_str());
                    return false;
                }

                return true;
            }

            /// @brief Remove only regular files in a directory.
            bool RemoveRegularFilesInDirectory(const std::string &directoryPath)
            {
                DIR *dir = ::opendir(directoryPath.c_str());
                if (dir == nullptr)
                {
                    return false;
                }

                bool result{true};
                struct dirent *entry;
                while ((entry = ::readdir(dir)) != nullptr)
                {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..")
                    {
                        continue;
                    }

                    const std::string fullPath{directoryPath + "/" + name};
                    struct stat st;
                    if (::stat(fullPath.c_str(), &st) != 0)
                    {
                        result = false;
                        continue;
                    }

                    if (S_ISREG(st.st_mode))
                    {
                        if (::remove(fullPath.c_str()) != 0)
                        {
                            result = false;
                        }
                    }
                }

                ::closedir(dir);
                return result;
            }

            /// @brief Copy only regular files from one directory to another.
            bool CopyRegularFiles(const std::string &sourceDir, const std::string &targetDir)
            {
                if (!IsDirectory(sourceDir) || !EnsureDirectoryExists(targetDir))
                {
                    return false;
                }

                DIR *dir = ::opendir(sourceDir.c_str());
                if (dir == nullptr)
                {
                    return false;
                }

                bool result{true};
                struct dirent *entry;
                while ((entry = ::readdir(dir)) != nullptr)
                {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..")
                    {
                        continue;
                    }

                    const std::string sourcePath{sourceDir + "/" + name};
                    struct stat st;
                    if (::stat(sourcePath.c_str(), &st) != 0)
                    {
                        result = false;
                        continue;
                    }

                    if (!S_ISREG(st.st_mode))
                    {
                        continue;
                    }

                    const std::string targetPath{targetDir + "/" + name};
                    if (!CopyFile(sourcePath, targetPath))
                    {
                        result = false;
                    }
                }

                ::closedir(dir);
                return result;
            }

            /// @brief Write schema version metadata atomically.
            bool WriteSchemaVersion(const std::string &basePath, std::uint16_t version)
            {
                const std::string schemaFilePath{basePath + "/" + cSchemaVersionFileName};
                const std::string tmpPath{schemaFilePath + ".tmp"};

                std::ofstream out(tmpPath, std::ios::trunc);
                if (!out.is_open())
                {
                    return false;
                }

                out << version;
                out.flush();
                if (!out.good())
                {
                    out.close();
                    ::remove(tmpPath.c_str());
                    return false;
                }
                out.close();

                if (::rename(tmpPath.c_str(), schemaFilePath.c_str()) != 0)
                {
                    ::remove(tmpPath.c_str());
                    return false;
                }

                return true;
            }

            /// @brief Read existing schema version metadata.
            bool ReadSchemaVersion(const std::string &basePath, std::uint16_t &version)
            {
                const std::string schemaFilePath{basePath + "/" + cSchemaVersionFileName};
                std::ifstream in(schemaFilePath);
                if (!in.is_open())
                {
                    return false;
                }

                std::uint32_t parsedVersion{0};
                in >> parsedVersion;
                if (!in.good() && !in.eof())
                {
                    return false;
                }

                if (parsedVersion > std::numeric_limits<std::uint16_t>::max())
                {
                    return false;
                }

                version = static_cast<std::uint16_t>(parsedVersion);
                return true;
            }

            /// @brief Ensure the persistency root and per-instance directory exist.
            bool EnsureStorageRoot(const std::string &basePath)
            {
                return EnsureDirectoryExists(cStorageRoot) &&
                       EnsureDirectoryExists(basePath);
            }
        } // namespace

        /// @brief Map an InstanceSpecifier to a filesystem path.
        ///        Uses /tmp/ara_per/<specifier>/ as the storage root.
        ///        Slashes in the specifier are replaced with underscores.
        static std::string SpecifierToPath(
            const core::InstanceSpecifier &specifier)
        {
            std::string path = specifier.ToString();
            std::replace(path.begin(), path.end(), '/', '_');
            return cStorageRoot + "/" + path;
        }

        core::Result<SharedHandle<KeyValueStorage>> OpenKeyValueStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<SharedHandle<KeyValueStorage>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::string filePath = basePath + "/" + cKeyValueStorageFileName;

            SharedHandle<KeyValueStorage> handle =
                std::make_shared<KeyValueStorage>(filePath);

            return core::Result<SharedHandle<KeyValueStorage>>::FromValue(
                std::move(handle));
        }

        core::Result<SharedHandle<FileStorage>> OpenFileStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<SharedHandle<FileStorage>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::string filesDir = basePath + "/" + cFileStorageDirName;
            if (!EnsureDirectoryExists(filesDir))
            {
                return core::Result<SharedHandle<FileStorage>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            SharedHandle<FileStorage> handle =
                std::make_shared<FileStorage>(filesDir);

            return core::Result<SharedHandle<FileStorage>>::FromValue(
                std::move(handle));
        }

        core::Result<void> RecoverKeyValueStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            const std::string filePath{
                basePath + "/" + cKeyValueStorageFileName};
            const std::string backupPath{
                basePath + "/" + cKeyValueStorageBackupFileName};

            if (PathExists(backupPath))
            {
                if (!CopyFile(backupPath, filePath))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PerErrc::kIntegrityCorrupted));
                }

                return core::Result<void>::FromValue();
            }

            if (!PathExists(filePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> ResetKeyValueStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::string filePath = basePath + "/" + cKeyValueStorageFileName;
            std::string backupPath = basePath + "/" + cKeyValueStorageBackupFileName;

            if (!RemoveFileIfExists(filePath) ||
                !RemoveFileIfExists(backupPath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }
            return core::Result<void>::FromValue();
        }

        core::Result<void> RecoverFileStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            const std::string filesDir{
                basePath + "/" + cFileStorageDirName};
            const std::string backupDir{
                basePath + "/" + cFileStorageBackupDirName};

            if (!EnsureDirectoryExists(filesDir))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            if (IsDirectory(backupDir))
            {
                if (!RemoveRegularFilesInDirectory(filesDir) ||
                    !CopyRegularFiles(backupDir, filesDir))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PerErrc::kIntegrityCorrupted));
                }
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> ResetFileStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            const std::string filesDir{
                basePath + "/" + cFileStorageDirName};
            if (!EnsureDirectoryExists(filesDir))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            if (!RemoveRegularFilesInDirectory(filesDir))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdatePersistency(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            if (!EnsureStorageRoot(basePath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            const std::string filesDir{
                basePath + "/" + cFileStorageDirName};
            const std::string backupDir{
                basePath + "/" + cFileStorageBackupDirName};
            if (!EnsureDirectoryExists(filesDir) ||
                !EnsureDirectoryExists(backupDir))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::uint16_t existingSchemaVersion{0};
            if (ReadSchemaVersion(basePath, existingSchemaVersion) &&
                (existingSchemaVersion > cPersistencySchemaVersion))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kValidationFailed));
            }

            auto kvStorageResult{OpenKeyValueStorage(specifier)};
            if (!kvStorageResult.HasValue())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            auto &storage{kvStorageResult.Value()};
            auto setSchemaResult{
                storage->SetValue<std::uint16_t>(
                    cSchemaVersionKey,
                    cPersistencySchemaVersion)};
            if (!setSchemaResult.HasValue())
            {
                return setSchemaResult;
            }

            auto setReleaseResult{
                storage->SetStringValue(
                    cReleaseProfileKey,
                    ara::core::ApReleaseInfo::cReleaseString)};
            if (!setReleaseResult.HasValue())
            {
                return setReleaseResult;
            }

            auto syncResult{storage->SyncToStorage()};
            if (!syncResult.HasValue())
            {
                return syncResult;
            }

            const std::string kvStoragePath{
                basePath + "/" + cKeyValueStorageFileName};
            const std::string kvStorageBackupPath{
                basePath + "/" + cKeyValueStorageBackupFileName};

            if (PathExists(kvStoragePath) &&
                !CopyFile(kvStoragePath, kvStorageBackupPath))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kIntegrityCorrupted));
            }

            if (!RemoveRegularFilesInDirectory(backupDir) ||
                !CopyRegularFiles(filesDir, backupDir))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kIntegrityCorrupted));
            }

            if (!WriteSchemaVersion(basePath, cPersistencySchemaVersion))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }
    }
}
