#include "./persistency.h"
#include <algorithm>
#include <cstdio>
#include <sys/stat.h>

namespace ara
{
    namespace per
    {
        /// @brief Map an InstanceSpecifier to a filesystem path.
        ///        Uses /tmp/ara_per/<specifier>/ as the storage root.
        ///        Slashes in the specifier are replaced with underscores.
        static std::string SpecifierToPath(
            const core::InstanceSpecifier &specifier)
        {
            std::string path = specifier.ToString();
            std::replace(path.begin(), path.end(), '/', '_');
            return "/tmp/ara_per/" + path;
        }

        static void EnsureDirectoryExists(const std::string &path)
        {
            ::mkdir("/tmp/ara_per", 0755);
            ::mkdir(path.c_str(), 0755);
        }

        core::Result<SharedHandle<KeyValueStorage>> OpenKeyValueStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            EnsureDirectoryExists(basePath);

            std::string filePath = basePath + "/kvs.dat";

            SharedHandle<KeyValueStorage> handle =
                std::make_shared<KeyValueStorage>(filePath);

            return core::Result<SharedHandle<KeyValueStorage>>::FromValue(
                std::move(handle));
        }

        core::Result<SharedHandle<FileStorage>> OpenFileStorage(
            const core::InstanceSpecifier &specifier)
        {
            std::string basePath = SpecifierToPath(specifier);
            EnsureDirectoryExists(basePath);

            std::string filesDir = basePath + "/files";
            ::mkdir(filesDir.c_str(), 0755);

            SharedHandle<FileStorage> handle =
                std::make_shared<FileStorage>(filesDir);

            return core::Result<SharedHandle<FileStorage>>::FromValue(
                std::move(handle));
        }

        core::Result<void> RecoverKeyValueStorage(
            const core::InstanceSpecifier &specifier)
        {
            // In this simplified implementation, recovery is a no-op
            // (no backup mechanism). A production implementation would
            // restore from a redundant copy.
            std::string basePath = SpecifierToPath(specifier);
            std::string filePath = basePath + "/kvs.dat";
            struct stat st;
            if (::stat(filePath.c_str(), &st) != 0)
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
            std::string filePath = basePath + "/kvs.dat";

            // Delete the storage file
            std::remove(filePath.c_str());
            return core::Result<void>::FromValue();
        }
    }
}
