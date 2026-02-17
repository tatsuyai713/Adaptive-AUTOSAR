#include <gtest/gtest.h>
#include "../../../src/ara/crypto/key_slot.h"

namespace ara
{
    namespace crypto
    {
        TEST(KeySlotTest, ConstructAndGetMetadata)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 256U, true};
            std::vector<std::uint8_t> _key{0x01, 0x02, 0x03};
            KeySlot _slot{_meta, _key};

            EXPECT_EQ(_slot.GetMetadata().SlotId, "slot1");
            EXPECT_EQ(_slot.GetMetadata().Type, KeySlotType::kSymmetric);
            EXPECT_EQ(_slot.GetMetadata().KeySizeBits, 256U);
            EXPECT_TRUE(_slot.GetMetadata().Exportable);
        }

        TEST(KeySlotTest, GetKeyMaterialExportable)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};
            std::vector<std::uint8_t> _key{0xAA, 0xBB, 0xCC};
            KeySlot _slot{_meta, _key};

            auto _result = _slot.GetKeyMaterial();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().size(), 3U);
            EXPECT_EQ(_result.Value()[0], 0xAA);
        }

        TEST(KeySlotTest, GetKeyMaterialNonExportable)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kRsaPrivate, 2048U, false};
            std::vector<std::uint8_t> _key{0x01};
            KeySlot _slot{_meta, _key};

            auto _result = _slot.GetKeyMaterial();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(KeySlotTest, IsEmptyOnConstruct)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};
            KeySlot _slot{_meta, {}};
            EXPECT_TRUE(_slot.IsEmpty());
        }

        TEST(KeySlotTest, UpdateAndClear)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};
            KeySlot _slot{_meta, {}};

            EXPECT_TRUE(_slot.IsEmpty());

            auto _updateResult = _slot.Update({0x01, 0x02});
            EXPECT_TRUE(_updateResult.HasValue());
            EXPECT_FALSE(_slot.IsEmpty());

            _slot.Clear();
            EXPECT_TRUE(_slot.IsEmpty());
        }

        TEST(KeySlotTest, UpdateWithEmptyFails)
        {
            KeySlotMetadata _meta{"slot1", KeySlotType::kSymmetric, 128U, true};
            KeySlot _slot{_meta, {0x01}};

            auto _result = _slot.Update({});
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
