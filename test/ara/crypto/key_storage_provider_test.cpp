#include <gtest/gtest.h>
#include "../../../src/ara/crypto/key_storage_provider.h"

namespace ara
{
    namespace crypto
    {
        TEST(KeyStorageProviderTest, CreateAndGetSlot)
        {
            KeyStorageProvider _provider;
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};

            auto _createResult = _provider.CreateSlot(_meta);
            EXPECT_TRUE(_createResult.HasValue());

            auto _getResult = _provider.GetSlot("slot1");
            ASSERT_TRUE(_getResult.HasValue());
            EXPECT_EQ(_getResult.Value()->GetMetadata().SlotId, "slot1");
        }

        TEST(KeyStorageProviderTest, CreateDuplicateFails)
        {
            KeyStorageProvider _provider;
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};

            _provider.CreateSlot(_meta);
            auto _result = _provider.CreateSlot(_meta);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(KeyStorageProviderTest, DeleteSlot)
        {
            KeyStorageProvider _provider;
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};
            _provider.CreateSlot(_meta);

            auto _deleteResult = _provider.DeleteSlot("slot1");
            EXPECT_TRUE(_deleteResult.HasValue());

            auto _getResult = _provider.GetSlot("slot1");
            EXPECT_FALSE(_getResult.HasValue());
        }

        TEST(KeyStorageProviderTest, DeleteNonexistentFails)
        {
            KeyStorageProvider _provider;
            auto _result = _provider.DeleteSlot("nonexistent");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(KeyStorageProviderTest, ListSlotIds)
        {
            KeyStorageProvider _provider;
            _provider.CreateSlot({"a", KeySlotType::kSymmetric, 128U, true});
            _provider.CreateSlot({"b", KeySlotType::kRsaPublic, 2048U, true});

            auto _ids = _provider.ListSlotIds();
            EXPECT_EQ(_ids.size(), 2U);
        }

        TEST(KeyStorageProviderTest, StoreAndRetrieveKey)
        {
            KeyStorageProvider _provider;
            _provider.CreateSlot({"slot1", KeySlotType::kSymmetric, 128U, true});

            std::vector<std::uint8_t> _key{0xDE, 0xAD, 0xBE, 0xEF};
            auto _storeResult = _provider.StoreKey("slot1", _key);
            EXPECT_TRUE(_storeResult.HasValue());

            auto _getResult = _provider.GetSlot("slot1");
            ASSERT_TRUE(_getResult.HasValue());
            auto _keyResult = _getResult.Value()->GetKeyMaterial();
            ASSERT_TRUE(_keyResult.HasValue());
            EXPECT_EQ(_keyResult.Value(), _key);
        }

        TEST(KeyStorageProviderTest, StoreToNonexistentFails)
        {
            KeyStorageProvider _provider;
            auto _result = _provider.StoreKey("nope", {0x01});
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(KeyStorageProviderTest, SaveAndLoadRoundTrip)
        {
            const std::string _dir{"/tmp/autosar_test_keystore"};

            // Create and populate provider
            KeyStorageProvider _provider;
            _provider.CreateSlot({"sym1", KeySlotType::kSymmetric, 128U, true});
            _provider.StoreKey("sym1", {0xCA, 0xFE, 0xBA, 0xBE});

            auto _saveResult = _provider.SaveToDirectory(_dir);
            EXPECT_TRUE(_saveResult.HasValue());

            // Load into fresh provider
            KeyStorageProvider _provider2;
            auto _loadResult = _provider2.LoadFromDirectory(_dir);
            EXPECT_TRUE(_loadResult.HasValue());

            auto _ids = _provider2.ListSlotIds();
            EXPECT_EQ(_ids.size(), 1U);

            auto _getResult = _provider2.GetSlot("sym1");
            ASSERT_TRUE(_getResult.HasValue());
            auto _keyResult = _getResult.Value()->GetKeyMaterial();
            ASSERT_TRUE(_keyResult.HasValue());
            EXPECT_EQ(_keyResult.Value().size(), 4U);
            EXPECT_EQ(_keyResult.Value()[0], 0xCA);
        }
    }
}
