#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include "../../../src/ara/per/key_value_storage.h"

namespace ara
{
    namespace per
    {
        static const std::string cBatchTestFilePath{"/tmp/ara_per_test_batch.dat"};

        class KeyValueStorageBatchTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::remove(cBatchTestFilePath.c_str());
                std::remove((cBatchTestFilePath + ".tmp").c_str());
            }
            void TearDown() override
            {
                std::remove(cBatchTestFilePath.c_str());
                std::remove((cBatchTestFilePath + ".tmp").c_str());
            }
        };

        TEST_F(KeyValueStorageBatchTest, SetValuesAndGetValues)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            std::map<std::string, std::int32_t> batch;
            batch["a"] = 10;
            batch["b"] = 20;
            batch["c"] = 30;

            auto setResult = storage.SetValues<std::int32_t>(batch);
            ASSERT_TRUE(setResult.HasValue());

            EXPECT_TRUE(storage.HasKey("a"));
            EXPECT_TRUE(storage.HasKey("b"));
            EXPECT_TRUE(storage.HasKey("c"));

            std::vector<std::string> keys{"a", "b", "c", "nonexistent"};
            auto getResult = storage.GetValues<std::int32_t>(keys);
            ASSERT_TRUE(getResult.HasValue());

            auto &vals = getResult.Value();
            EXPECT_EQ(vals.size(), 3U);
            EXPECT_EQ(vals["a"], 10);
            EXPECT_EQ(vals["b"], 20);
            EXPECT_EQ(vals["c"], 30);
            EXPECT_EQ(vals.find("nonexistent"), vals.end());
        }

        TEST_F(KeyValueStorageBatchTest, SetStringValues)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            std::map<std::string, std::string> batch;
            batch["name"] = "AUTOSAR";
            batch["version"] = "R24-11";

            auto result = storage.SetStringValues(batch);
            ASSERT_TRUE(result.HasValue());

            auto name = storage.GetStringValue("name");
            ASSERT_TRUE(name.HasValue());
            EXPECT_EQ(name.Value(), "AUTOSAR");

            auto ver = storage.GetStringValue("version");
            ASSERT_TRUE(ver.HasValue());
            EXPECT_EQ(ver.Value(), "R24-11");
        }

        TEST_F(KeyValueStorageBatchTest, GetStringValues)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            storage.SetStringValue("k1", "v1");
            storage.SetStringValue("k2", "v2");

            auto result = storage.GetStringValues({"k1", "k2", "k3"});
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value().size(), 2U);
            EXPECT_EQ(result.Value().at("k1"), "v1");
            EXPECT_EQ(result.Value().at("k2"), "v2");
        }

        TEST_F(KeyValueStorageBatchTest, RemoveKeys)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            storage.SetValue<int>("x", 1);
            storage.SetValue<int>("y", 2);
            storage.SetValue<int>("z", 3);

            auto result = storage.RemoveKeys({"x", "z", "nonexistent"});
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 2U);

            EXPECT_FALSE(storage.HasKey("x"));
            EXPECT_TRUE(storage.HasKey("y"));
            EXPECT_FALSE(storage.HasKey("z"));
        }

        TEST_F(KeyValueStorageBatchTest, BatchObserverNotification)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            std::vector<std::string> notifiedKeys;
            storage.SetGlobalObserver(
                [&](const std::string &key)
                {
                    notifiedKeys.push_back(key);
                });

            std::map<std::string, int> batch;
            batch["a"] = 1;
            batch["b"] = 2;
            storage.SetValues<int>(batch);

            EXPECT_EQ(notifiedKeys.size(), 2U);
        }

        TEST_F(KeyValueStorageBatchTest, PendingChangeCountBatch)
        {
            KeyValueStorage storage(cBatchTestFilePath);

            std::map<std::string, int> batch;
            batch["a"] = 1;
            batch["b"] = 2;
            batch["c"] = 3;
            storage.SetValues<int>(batch);

            EXPECT_EQ(storage.GetPendingChangeCount(), 3U);
        }
    }
}
