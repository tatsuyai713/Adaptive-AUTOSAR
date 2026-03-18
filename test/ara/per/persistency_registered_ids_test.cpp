#include <gtest/gtest.h>
#include "../../../src/ara/per/persistency.h"

namespace ara
{
    namespace per
    {
        TEST(PersistencyRegisteredIdsTest, GetRegisteredKvsIdsEmptyByDefault)
        {
            auto _result = GetRegisteredKvsIds();
            ASSERT_TRUE(_result.HasValue());
            // May or may not be empty depending on what other tests left behind,
            // but the call must succeed.
        }

        TEST(PersistencyRegisteredIdsTest, GetRegisteredFileStorageIdsEmptyByDefault)
        {
            auto _result = GetRegisteredFileStorageIds();
            ASSERT_TRUE(_result.HasValue());
        }

        TEST(PersistencyRegisteredIdsTest, OpenKvsAppearsInRegisteredIds)
        {
            const core::InstanceSpecifier _spec("registered_kvs_test_id");

            // Create a KVS to register it.
            auto _kvsResult = OpenKeyValueStorage(_spec);
            ASSERT_TRUE(_kvsResult.HasValue());

            // Write something to make sure the file exists.
            auto &_kvs = _kvsResult.Value();
            _kvs->SetStringValue("test_key", "test_value");
            _kvs->SyncToStorage();

            auto _idsResult = GetRegisteredKvsIds();
            ASSERT_TRUE(_idsResult.HasValue());
            const auto &_ids = _idsResult.Value();

            // The specifier (with / replaced by _) should appear.
            bool _found{false};
            for (const auto &id : _ids)
            {
                if (id == "registered_kvs_test_id")
                {
                    _found = true;
                    break;
                }
            }
            EXPECT_TRUE(_found);

            // Clean up.
            ResetKeyValueStorage(_spec);
        }

        TEST(PersistencyRegisteredIdsTest, OpenFileStorageAppearsInRegisteredIds)
        {
            const core::InstanceSpecifier _spec("registered_fs_test_id");

            auto _fsResult = OpenFileStorage(_spec);
            ASSERT_TRUE(_fsResult.HasValue());

            auto _idsResult = GetRegisteredFileStorageIds();
            ASSERT_TRUE(_idsResult.HasValue());
            const auto &_ids = _idsResult.Value();

            bool _found{false};
            for (const auto &id : _ids)
            {
                if (id == "registered_fs_test_id")
                {
                    _found = true;
                    break;
                }
            }
            EXPECT_TRUE(_found);

            // Clean up.
            ResetFileStorage(_spec);
        }
    }
}
