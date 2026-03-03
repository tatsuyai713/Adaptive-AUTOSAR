#include <gtest/gtest.h>
#include <cstdio>
#include "../../../../src/ara/com/secoc/secoc_key_manager.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            namespace
            {
                // Populate a KeyStorageProvider with one exportable symmetric slot.
                void InitProvider(
                    ara::crypto::KeyStorageProvider &ksp,
                    const std::string &slotId,
                    const std::vector<std::uint8_t> &keyBytes)
                {
                    ara::crypto::KeySlotMetadata md;
                    md.SlotId = slotId;
                    md.Type = ara::crypto::KeySlotType::kSymmetric;
                    md.KeySizeBits = static_cast<std::uint32_t>(keyBytes.size() * 8U);
                    md.Exportable = true;
                    (void)ksp.CreateSlot(md);
                    (void)ksp.StoreKey(slotId, keyBytes);
                }
            }

            TEST(SecOcKeyManagerTest, RegisterAndGetKey)
            {
                const std::vector<std::uint8_t> cKey(16U, 0xAB);
                ara::crypto::KeyStorageProvider _ksp;
                InitProvider(_ksp, "slot_0x100", cKey);
                SecOcKeyManager _km{_ksp};

                ASSERT_TRUE(_km.RegisterKey(0x100, "slot_0x100").HasValue());
                EXPECT_TRUE(_km.IsKeyRegistered(0x100));

                const auto _result{_km.GetKey(0x100)};
                ASSERT_TRUE(_result.HasValue());
                EXPECT_EQ(_result.Value(), cKey);
            }

            TEST(SecOcKeyManagerTest, GetKeyCachesOnSecondCall)
            {
                const std::vector<std::uint8_t> cKey(32U, 0x77);
                ara::crypto::KeyStorageProvider _ksp;
                InitProvider(_ksp, "slot_enc", cKey);
                SecOcKeyManager _km{_ksp};

                ASSERT_TRUE(_km.RegisterKey(0x01, "slot_enc").HasValue());
                const auto _r1{_km.GetKey(0x01)};
                const auto _r2{_km.GetKey(0x01)};
                ASSERT_TRUE(_r1.HasValue());
                ASSERT_TRUE(_r2.HasValue());
                EXPECT_EQ(_r1.Value(), _r2.Value());
            }

            TEST(SecOcKeyManagerTest, GetKeyUnregisteredFails)
            {
                ara::crypto::KeyStorageProvider _ksp;
                SecOcKeyManager _km{_ksp};

                const auto _result{_km.GetKey(0xFF)};
                ASSERT_FALSE(_result.HasValue());
                EXPECT_EQ(
                    static_cast<SecOcErrc>(_result.Error().Value()),
                    SecOcErrc::kKeyNotFound);
            }

            TEST(SecOcKeyManagerTest, RegisterDuplicatePduFails)
            {
                const std::vector<std::uint8_t> cKey(16U, 0x01);
                ara::crypto::KeyStorageProvider _ksp;
                InitProvider(_ksp, "slot_dup", cKey);
                SecOcKeyManager _km{_ksp};

                ASSERT_TRUE(_km.RegisterKey(0x200, "slot_dup").HasValue());
                EXPECT_FALSE(_km.RegisterKey(0x200, "slot_dup").HasValue());
            }

            TEST(SecOcKeyManagerTest, RegisterUnknownSlotFails)
            {
                ara::crypto::KeyStorageProvider _ksp;
                SecOcKeyManager _km{_ksp};

                const auto _result{_km.RegisterKey(0x300, "no_such_slot")};
                ASSERT_FALSE(_result.HasValue());
                EXPECT_EQ(
                    static_cast<SecOcErrc>(_result.Error().Value()),
                    SecOcErrc::kKeyNotFound);
            }

            TEST(SecOcKeyManagerTest, UnregisterKey)
            {
                const std::vector<std::uint8_t> cKey(16U, 0x55);
                ara::crypto::KeyStorageProvider _ksp;
                InitProvider(_ksp, "slot_tmp", cKey);
                SecOcKeyManager _km{_ksp};

                ASSERT_TRUE(_km.RegisterKey(0x400, "slot_tmp").HasValue());
                ASSERT_TRUE(_km.IsKeyRegistered(0x400));
                ASSERT_TRUE(_km.UnregisterKey(0x400).HasValue());
                EXPECT_FALSE(_km.IsKeyRegistered(0x400));
            }

            TEST(SecOcKeyManagerTest, UnregisterUnknownFails)
            {
                ara::crypto::KeyStorageProvider _ksp;
                SecOcKeyManager _km{_ksp};

                EXPECT_FALSE(_km.UnregisterKey(0x500).HasValue());
            }

            TEST(SecOcKeyManagerTest, RefreshKeyLoadsNewMaterial)
            {
                const std::string cSlotId{"slot_refresh"};
                const std::vector<std::uint8_t> cKeyV1(16U, 0x11);
                const std::vector<std::uint8_t> cKeyV2(16U, 0x22);

                ara::crypto::KeyStorageProvider _ksp;
                InitProvider(_ksp, cSlotId, cKeyV1);
                SecOcKeyManager _km{_ksp};

                ASSERT_TRUE(_km.RegisterKey(0x600, cSlotId).HasValue());

                // Load v1 into cache
                const auto _r1{_km.GetKey(0x600)};
                ASSERT_TRUE(_r1.HasValue());
                EXPECT_EQ(_r1.Value(), cKeyV1);

                // Update the slot with v2
                (void)_ksp.StoreKey(cSlotId, cKeyV2);

                // Refresh forces reload
                ASSERT_TRUE(_km.RefreshKey(0x600).HasValue());
                const auto _r2{_km.GetKey(0x600)};
                ASSERT_TRUE(_r2.HasValue());
                EXPECT_EQ(_r2.Value(), cKeyV2);
            }

            // --- FreshnessManager persistence tests ---

            TEST(FreshnessManagerPersistenceTest, SaveAndLoadRoundTrip)
            {
                const std::string cPath{"/tmp/ara_secoc_freshness_test.txt"};

                {
                    FreshnessManager _fm;
                    ASSERT_TRUE(_fm.RegisterPdu(0x01, {4, 0}));
                    ASSERT_TRUE(_fm.RegisterPdu(0x02, {4, 0}));

                    for (int i = 0; i < 5; ++i)
                    {
                        (void)_fm.IncrementCounter(0x01);
                    }
                    for (int i = 0; i < 3; ++i)
                    {
                        (void)_fm.IncrementCounter(0x02);
                    }

                    ASSERT_TRUE(_fm.SaveToFile(cPath).HasValue());
                }

                {
                    FreshnessManager _fm;
                    ASSERT_TRUE(_fm.RegisterPdu(0x01, {4, 0}));
                    ASSERT_TRUE(_fm.RegisterPdu(0x02, {4, 0}));

                    ASSERT_TRUE(_fm.LoadFromFile(cPath).HasValue());

                    const auto _v1{_fm.GetCounterValue(0x01)};
                    ASSERT_TRUE(_v1.HasValue());
                    EXPECT_EQ(_v1.Value(), 5U);

                    const auto _v2{_fm.GetCounterValue(0x02)};
                    ASSERT_TRUE(_v2.HasValue());
                    EXPECT_EQ(_v2.Value(), 3U);
                }

                std::remove(cPath.c_str());
            }

            TEST(FreshnessManagerPersistenceTest, LoadIgnoresUnregisteredPdus)
            {
                const std::string cPath{"/tmp/ara_secoc_freshness_unknown_test.txt"};

                {
                    FreshnessManager _fm;
                    ASSERT_TRUE(_fm.RegisterPdu(0x10, {4, 0}));
                    (void)_fm.IncrementCounter(0x10);
                    ASSERT_TRUE(_fm.SaveToFile(cPath).HasValue());
                }

                {
                    FreshnessManager _fm;
                    // PDU 0x10 is NOT registered — file entry should be silently ignored
                    ASSERT_TRUE(_fm.LoadFromFile(cPath).HasValue());
                    // No crash, no error
                }

                std::remove(cPath.c_str());
            }
        }
    }
}
