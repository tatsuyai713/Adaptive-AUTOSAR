#include <gtest/gtest.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include "../../../src/ara/per/key_value_storage.h"

namespace ara
{
    namespace per
    {
        static const std::string cTestFilePath{"/tmp/ara_per_test_kvs.dat"};

        class KeyValueStorageTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                // Clean up any leftover test file
                std::remove(cTestFilePath.c_str());
                std::remove((cTestFilePath + ".tmp").c_str());
            }

            void TearDown() override
            {
                std::remove(cTestFilePath.c_str());
                std::remove((cTestFilePath + ".tmp").c_str());
            }
        };

        TEST_F(KeyValueStorageTest, SetAndGetInt)
        {
            KeyValueStorage storage(cTestFilePath);

            std::int32_t value = 42;
            auto setResult = storage.SetValue<std::int32_t>("myInt", value);
            ASSERT_TRUE(setResult.HasValue());

            auto getResult = storage.GetValue<std::int32_t>("myInt");
            ASSERT_TRUE(getResult.HasValue());
            EXPECT_EQ(getResult.Value(), 42);
        }

        TEST_F(KeyValueStorageTest, SetAndGetDouble)
        {
            KeyValueStorage storage(cTestFilePath);

            double value = 3.14;
            storage.SetValue<double>("pi", value);

            auto result = storage.GetValue<double>("pi");
            ASSERT_TRUE(result.HasValue());
            EXPECT_DOUBLE_EQ(result.Value(), 3.14);
        }

        TEST_F(KeyValueStorageTest, SetAndGetString)
        {
            KeyValueStorage storage(cTestFilePath);

            storage.SetStringValue("name", "AUTOSAR");

            auto result = storage.GetStringValue("name");
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), "AUTOSAR");
        }

        TEST_F(KeyValueStorageTest, GetNonExistentKey)
        {
            KeyValueStorage storage(cTestFilePath);

            auto result = storage.GetValue<int>("nonexistent");
            EXPECT_FALSE(result.HasValue());
        }

        TEST_F(KeyValueStorageTest, HasKey)
        {
            KeyValueStorage storage(cTestFilePath);

            EXPECT_FALSE(storage.HasKey("key1"));
            storage.SetValue<int>("key1", 1);
            EXPECT_TRUE(storage.HasKey("key1"));
        }

        TEST_F(KeyValueStorageTest, RemoveKey)
        {
            KeyValueStorage storage(cTestFilePath);

            storage.SetValue<int>("toRemove", 100);
            EXPECT_TRUE(storage.HasKey("toRemove"));

            auto result = storage.RemoveKey("toRemove");
            ASSERT_TRUE(result.HasValue());
            EXPECT_FALSE(storage.HasKey("toRemove"));
        }

        TEST_F(KeyValueStorageTest, RemoveNonExistentKey)
        {
            KeyValueStorage storage(cTestFilePath);

            auto result = storage.RemoveKey("nonexistent");
            EXPECT_FALSE(result.HasValue());
        }

        TEST_F(KeyValueStorageTest, GetAllKeys)
        {
            KeyValueStorage storage(cTestFilePath);

            storage.SetValue<int>("a", 1);
            storage.SetValue<int>("b", 2);
            storage.SetValue<int>("c", 3);

            auto result = storage.GetAllKeys();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value().size(), 3U);
        }

        TEST_F(KeyValueStorageTest, SyncAndReload)
        {
            // Write and sync
            {
                KeyValueStorage storage(cTestFilePath);
                storage.SetValue<std::uint32_t>("counter", 12345U);
                storage.SetStringValue("label", "test_value");
                storage.SyncToStorage();
            }

            // Reload from file
            {
                KeyValueStorage storage(cTestFilePath);
                auto intResult = storage.GetValue<std::uint32_t>("counter");
                ASSERT_TRUE(intResult.HasValue());
                EXPECT_EQ(intResult.Value(), 12345U);

                auto strResult = storage.GetStringValue("label");
                ASSERT_TRUE(strResult.HasValue());
                EXPECT_EQ(strResult.Value(), "test_value");
            }
        }

        TEST_F(KeyValueStorageTest, DiscardPendingChanges)
        {
            KeyValueStorage storage(cTestFilePath);

            storage.SetValue<int>("original", 1);
            storage.SyncToStorage();

            storage.SetValue<int>("original", 999);
            storage.SetValue<int>("newKey", 2);
            EXPECT_EQ(storage.GetValue<int>("original").Value(), 999);

            storage.DiscardPendingChanges();

            EXPECT_EQ(storage.GetValue<int>("original").Value(), 1);
            EXPECT_FALSE(storage.HasKey("newKey"));
        }

        TEST_F(KeyValueStorageTest, OverwriteExistingKey)
        {
            KeyValueStorage storage(cTestFilePath);

            storage.SetValue<int>("key", 1);
            EXPECT_EQ(storage.GetValue<int>("key").Value(), 1);

            storage.SetValue<int>("key", 2);
            EXPECT_EQ(storage.GetValue<int>("key").Value(), 2);
        }

        TEST_F(KeyValueStorageTest, EmptyStorageHasNoKeys)
        {
            KeyValueStorage storage(cTestFilePath);

            auto result = storage.GetAllKeys();
            ASSERT_TRUE(result.HasValue());
            EXPECT_TRUE(result.Value().empty());
        }
    }
}
