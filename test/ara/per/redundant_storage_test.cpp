#include <gtest/gtest.h>
#include "../../../src/ara/per/redundant_storage.h"

namespace ara
{
    namespace per
    {
        TEST(RedundantStorageTest, SetAndGetValue)
        {
            RedundantStorage _rs;
            std::vector<uint8_t> _val = {0x01, 0x02, 0x03};
            auto _r = _rs.SetValue("key1", _val);
            EXPECT_TRUE(_r.HasValue());
            auto _got = _rs.GetValue("key1");
            EXPECT_TRUE(_got.HasValue());
            EXPECT_EQ(_got.Value(), _val);
        }

        TEST(RedundantStorageTest, HasKey)
        {
            RedundantStorage _rs;
            EXPECT_FALSE(_rs.HasKey("key1"));
            _rs.SetValue("key1", {0x01});
            EXPECT_TRUE(_rs.HasKey("key1"));
        }

        TEST(RedundantStorageTest, RemoveKey)
        {
            RedundantStorage _rs;
            _rs.SetValue("key1", {0x01});
            auto _r = _rs.RemoveKey("key1");
            EXPECT_TRUE(_r.HasValue());
            EXPECT_FALSE(_rs.HasKey("key1"));
        }

        TEST(RedundantStorageTest, RemoveNonexistentFails)
        {
            RedundantStorage _rs;
            auto _r = _rs.RemoveKey("ghost");
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(RedundantStorageTest, GetAllKeys)
        {
            RedundantStorage _rs;
            _rs.SetValue("a", {0x01});
            _rs.SetValue("b", {0x02});
            auto _keys = _rs.GetAllKeys();
            EXPECT_EQ(_keys.size(), 2U);
        }

        TEST(RedundantStorageTest, FailoverToSecondary)
        {
            RedundantStorage _rs;
            _rs.SetValue("key1", {0x01, 0x02});
            _rs.SyncPartitions();
            // InjectPrimaryCorruption auto-failovers to secondary
            _rs.InjectPrimaryCorruption();
            auto _status = _rs.GetStatus();
            EXPECT_EQ(_status.Active, ActivePartition::kSecondary);
            auto _got = _rs.GetValue("key1");
            EXPECT_TRUE(_got.HasValue());
            EXPECT_EQ(_got.Value(), (std::vector<uint8_t>{0x01, 0x02}));
        }

        TEST(RedundantStorageTest, SyncPartitions)
        {
            RedundantStorage _rs;
            _rs.SetValue("key1", {0x01});
            auto _r = _rs.SyncPartitions();
            EXPECT_TRUE(_r.HasValue());
        }

        TEST(RedundantStorageTest, GetStatus)
        {
            RedundantStorage _rs;
            auto _status = _rs.GetStatus();
            EXPECT_EQ(_status.Active, ActivePartition::kPrimary);
            EXPECT_EQ(_status.PrimaryHealth, PartitionHealth::kHealthy);
        }

        TEST(RedundantStorageTest, Reset)
        {
            RedundantStorage _rs;
            _rs.SetValue("key1", {0x01});
            _rs.Reset();
            EXPECT_FALSE(_rs.HasKey("key1"));
            auto _status = _rs.GetStatus();
            EXPECT_EQ(_status.Active, ActivePartition::kPrimary);
            EXPECT_EQ(_status.PrimaryHealth, PartitionHealth::kHealthy);
        }

        TEST(RedundantStorageTest, GetNonexistentKeyFails)
        {
            RedundantStorage _rs;
            auto _r = _rs.GetValue("ghost");
            EXPECT_FALSE(_r.HasValue());
        }
    }
}
