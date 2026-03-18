#include <gtest/gtest.h>
#include "../../../src/ara/crypto/hsm_provider.h"

namespace ara
{
    namespace crypto
    {
        TEST(HsmProviderTest, InitializeSuccess)
        {
            HsmProvider _hsm;
            auto _result = _hsm.Initialize("TestSlot");
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(HsmProviderTest, GenerateKeyWithoutInitFails)
        {
            HsmProvider _hsm;
            auto _result = _hsm.GenerateKey(HsmAlgorithm::kAes128, "key1");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(HsmProviderTest, GenerateKeyAfterInit)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            auto _result = _hsm.GenerateKey(HsmAlgorithm::kAes256, "key1");
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(HsmProviderTest, ImportKey)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            std::vector<uint8_t> _material = {0x01, 0x02, 0x03, 0x04};
            auto _result = _hsm.ImportKey(HsmSlotType::kSymmetric, "imported", _material);
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(HsmProviderTest, EncryptDecryptRoundTrip)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            auto _genResult = _hsm.GenerateKey(HsmAlgorithm::kAes128, "key1");
            ASSERT_TRUE(_genResult.HasValue());
            uint32_t _slotId = _genResult.Value();
            std::vector<uint8_t> _plaintext = {0xDE, 0xAD, 0xBE, 0xEF};
            auto _encrypted = _hsm.Encrypt(_slotId, _plaintext);
            ASSERT_TRUE(_encrypted.Success);
            auto _decrypted = _hsm.Decrypt(_slotId, _encrypted.OutputData);
            ASSERT_TRUE(_decrypted.Success);
            EXPECT_EQ(_decrypted.OutputData, _plaintext);
        }

        TEST(HsmProviderTest, EncryptUnknownSlotFails)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            auto _result = _hsm.Encrypt(9999, {0x01});
            EXPECT_FALSE(_result.Success);
        }

        TEST(HsmProviderTest, SignAndVerify)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            auto _genResult = _hsm.GenerateKey(HsmAlgorithm::kHmacSha256, "signkey");
            ASSERT_TRUE(_genResult.HasValue());
            uint32_t _slotId = _genResult.Value();
            std::vector<uint8_t> _data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
            auto _sig = _hsm.Sign(_slotId, _data);
            ASSERT_TRUE(_sig.Success);
            bool _valid = _hsm.Verify(_slotId, _data, _sig.OutputData);
            EXPECT_TRUE(_valid);
        }

        TEST(HsmProviderTest, VerifyTamperedFails)
        {
            HsmProvider _hsm;
            _hsm.Initialize("TestSlot");
            auto _genResult = _hsm.GenerateKey(HsmAlgorithm::kHmacSha256, "signkey");
            ASSERT_TRUE(_genResult.HasValue());
            uint32_t _slotId = _genResult.Value();
            std::vector<uint8_t> _data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
            auto _sig = _hsm.Sign(_slotId, _data);
            ASSERT_TRUE(_sig.Success);
            std::vector<uint8_t> _tampered = {0x00, 0x00};
            bool _valid = _hsm.Verify(_slotId, _tampered, _sig.OutputData);
            EXPECT_FALSE(_valid);
        }
    }
}
