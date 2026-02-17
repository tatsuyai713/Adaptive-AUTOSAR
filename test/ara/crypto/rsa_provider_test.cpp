#include <gtest/gtest.h>
#include "../../../src/ara/crypto/rsa_provider.h"

namespace ara
{
    namespace crypto
    {
        TEST(RsaProviderTest, GenerateKeyPair2048)
        {
            auto _result = GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value().PublicKeyDer.empty());
            EXPECT_FALSE(_result.Value().PrivateKeyDer.empty());
        }

        TEST(RsaProviderTest, InvalidKeySizeFails)
        {
            auto _result = GenerateRsaKeyPair(1024U);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(RsaProviderTest, SignAndVerifyRoundTrip)
        {
            auto _keyResult = GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_keyResult.HasValue());

            std::vector<std::uint8_t> _data{
                0x48, 0x65, 0x6C, 0x6C, 0x6F};

            auto _sigResult = RsaSign(
                _data, _keyResult.Value().PrivateKeyDer);
            ASSERT_TRUE(_sigResult.HasValue());
            EXPECT_FALSE(_sigResult.Value().empty());

            auto _verifyResult = RsaVerify(
                _data, _sigResult.Value(),
                _keyResult.Value().PublicKeyDer);
            ASSERT_TRUE(_verifyResult.HasValue());
            EXPECT_TRUE(_verifyResult.Value());
        }

        TEST(RsaProviderTest, VerifyWithWrongKeyFails)
        {
            auto _key1 = GenerateRsaKeyPair(2048U);
            auto _key2 = GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_key1.HasValue());
            ASSERT_TRUE(_key2.HasValue());

            std::vector<std::uint8_t> _data{0x01, 0x02, 0x03};
            auto _sig = RsaSign(_data, _key1.Value().PrivateKeyDer);
            ASSERT_TRUE(_sig.HasValue());

            auto _result = RsaVerify(
                _data, _sig.Value(), _key2.Value().PublicKeyDer);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value());
        }

        TEST(RsaProviderTest, EncryptDecryptRoundTrip)
        {
            auto _keyResult = GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_keyResult.HasValue());

            std::vector<std::uint8_t> _plaintext{
                0x53, 0x65, 0x63, 0x72, 0x65, 0x74};

            auto _encResult = RsaEncrypt(
                _plaintext, _keyResult.Value().PublicKeyDer);
            ASSERT_TRUE(_encResult.HasValue());
            EXPECT_NE(_encResult.Value(), _plaintext);

            auto _decResult = RsaDecrypt(
                _encResult.Value(), _keyResult.Value().PrivateKeyDer);
            ASSERT_TRUE(_decResult.HasValue());
            EXPECT_EQ(_decResult.Value(), _plaintext);
        }

        TEST(RsaProviderTest, InvalidKeyFormatFails)
        {
            std::vector<std::uint8_t> _badKey{0x00, 0x01, 0x02};
            std::vector<std::uint8_t> _data{0x01};

            auto _result = RsaSign(_data, _badKey);
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
