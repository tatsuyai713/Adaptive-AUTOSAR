#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "../../../src/ara/core/ap_release_info.h"
#include "../../../src/ara/per/persistency.h"

namespace ara
{
    namespace per
    {
        namespace
        {
            std::string SpecifierToPath(std::string specifier)
            {
                std::replace(specifier.begin(), specifier.end(), '/', '_');
                return std::string("/tmp/ara_per/") + specifier;
            }

            bool FileExists(const std::string &path)
            {
                struct stat st;
                return ::stat(path.c_str(), &st) == 0;
            }

            bool IsDirectory(const std::string &path)
            {
                struct stat st;
                return (::stat(path.c_str(), &st) == 0) &&
                       S_ISDIR(st.st_mode);
            }

            void RemoveTree(const std::string &path)
            {
                struct stat st;
                if (::stat(path.c_str(), &st) != 0)
                {
                    return;
                }

                if (S_ISDIR(st.st_mode))
                {
                    DIR *dir = ::opendir(path.c_str());
                    if (dir == nullptr)
                    {
                        return;
                    }

                    struct dirent *entry;
                    while ((entry = ::readdir(dir)) != nullptr)
                    {
                        const std::string name{entry->d_name};
                        if (name == "." || name == "..")
                        {
                            continue;
                        }

                        RemoveTree(path + "/" + name);
                    }

                    ::closedir(dir);
                    ::rmdir(path.c_str());
                    return;
                }

                ::remove(path.c_str());
            }
        } // namespace

        TEST(PersistencyApiTest, UpdatePersistencyWritesMetadataAndBackups)
        {
            const std::string cSpecifierText{"Per/PersistencyUpdate"};
            const std::string cStoragePath{SpecifierToPath(cSpecifierText)};
            RemoveTree(cStoragePath);

            auto specifierResult = core::InstanceSpecifier::Create(cSpecifierText);
            ASSERT_TRUE(specifierResult.HasValue());
            auto specifier = specifierResult.Value();

            auto kvsResult = OpenKeyValueStorage(specifier);
            ASSERT_TRUE(kvsResult.HasValue());
            ASSERT_TRUE(kvsResult.Value()->SetStringValue("user_key", "hello").HasValue());
            ASSERT_TRUE(kvsResult.Value()->SyncToStorage().HasValue());

            auto fileStorageResult = OpenFileStorage(specifier);
            ASSERT_TRUE(fileStorageResult.HasValue());

            auto writerResult = fileStorageResult.Value()->OpenFileReadWrite("sample.bin");
            ASSERT_TRUE(writerResult.HasValue());
            const std::array<std::uint8_t, 3> cPayload{{0x11, 0x22, 0x33}};
            ASSERT_TRUE(writerResult.Value()->Write(cPayload.data(), cPayload.size()).HasValue());
            ASSERT_TRUE(writerResult.Value()->Sync().HasValue());

            auto updateResult = UpdatePersistency(specifier);
            ASSERT_TRUE(updateResult.HasValue());

            EXPECT_TRUE(FileExists(cStoragePath + "/schema.version"));
            EXPECT_TRUE(FileExists(cStoragePath + "/kvs.dat.bak"));
            EXPECT_TRUE(IsDirectory(cStoragePath + "/files.bak"));

            KeyValueStorage reloadedStorage(cStoragePath + "/kvs.dat");
            auto schemaVersionResult =
                reloadedStorage.GetValue<std::uint16_t>("__ara_per_schema_version");
            ASSERT_TRUE(schemaVersionResult.HasValue());
            EXPECT_EQ(schemaVersionResult.Value(), core::ApReleaseInfo::cReleaseCompact);

            auto releaseProfileResult =
                reloadedStorage.GetStringValue("__ara_ap_release_profile");
            ASSERT_TRUE(releaseProfileResult.HasValue());
            EXPECT_EQ(releaseProfileResult.Value(), core::ApReleaseInfo::cReleaseString);

            RemoveTree(cStoragePath);
        }

        TEST(PersistencyApiTest, RecoverKeyValueStorageRestoresBackupFile)
        {
            const std::string cSpecifierText{"Per/RecoverKvs"};
            const std::string cStoragePath{SpecifierToPath(cSpecifierText)};
            RemoveTree(cStoragePath);

            auto specifierResult = core::InstanceSpecifier::Create(cSpecifierText);
            ASSERT_TRUE(specifierResult.HasValue());
            auto specifier = specifierResult.Value();

            auto kvsResult = OpenKeyValueStorage(specifier);
            ASSERT_TRUE(kvsResult.HasValue());
            ASSERT_TRUE(kvsResult.Value()->SetValue<std::uint32_t>("counter", 42U).HasValue());
            ASSERT_TRUE(kvsResult.Value()->SyncToStorage().HasValue());

            ASSERT_TRUE(UpdatePersistency(specifier).HasValue());

            std::ofstream corruptFile(cStoragePath + "/kvs.dat", std::ios::trunc);
            ASSERT_TRUE(corruptFile.is_open());
            corruptFile << "corrupted";
            corruptFile.close();

            auto recoverResult = RecoverKeyValueStorage(specifier);
            ASSERT_TRUE(recoverResult.HasValue());

            KeyValueStorage recoveredStorage(cStoragePath + "/kvs.dat");
            auto counterResult = recoveredStorage.GetValue<std::uint32_t>("counter");
            ASSERT_TRUE(counterResult.HasValue());
            EXPECT_EQ(counterResult.Value(), 42U);

            RemoveTree(cStoragePath);
        }

        TEST(PersistencyApiTest, RecoverFileStorageRestoresBackupSnapshot)
        {
            const std::string cSpecifierText{"Per/RecoverFiles"};
            const std::string cStoragePath{SpecifierToPath(cSpecifierText)};
            RemoveTree(cStoragePath);

            auto specifierResult = core::InstanceSpecifier::Create(cSpecifierText);
            ASSERT_TRUE(specifierResult.HasValue());
            auto specifier = specifierResult.Value();

            auto fileStorageResult = OpenFileStorage(specifier);
            ASSERT_TRUE(fileStorageResult.HasValue());

            auto writerResult = fileStorageResult.Value()->OpenFileReadWrite("payload.bin");
            ASSERT_TRUE(writerResult.HasValue());
            const std::array<std::uint8_t, 4> cExpectedPayload{{0x01, 0x02, 0x03, 0x04}};
            ASSERT_TRUE(writerResult.Value()->Write(cExpectedPayload.data(), cExpectedPayload.size()).HasValue());
            ASSERT_TRUE(writerResult.Value()->Sync().HasValue());

            ASSERT_TRUE(UpdatePersistency(specifier).HasValue());
            ASSERT_TRUE(ResetFileStorage(specifier).HasValue());
            EXPECT_FALSE(FileExists(cStoragePath + "/files/payload.bin"));

            auto recoverResult = RecoverFileStorage(specifier);
            ASSERT_TRUE(recoverResult.HasValue());

            auto readerResult = fileStorageResult.Value()->OpenFileReadOnly("payload.bin");
            ASSERT_TRUE(readerResult.HasValue());
            std::array<std::uint8_t, 4> actualPayload{{0x00, 0x00, 0x00, 0x00}};
            auto readResult = readerResult.Value()->Read(
                actualPayload.data(),
                actualPayload.size());
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(readResult.Value(), cExpectedPayload.size());
            EXPECT_EQ(actualPayload, cExpectedPayload);

            RemoveTree(cStoragePath);
        }
    }
}
