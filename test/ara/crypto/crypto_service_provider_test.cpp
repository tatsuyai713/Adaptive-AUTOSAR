#include <gtest/gtest.h>
#include "../../../src/ara/crypto/crypto_service_provider.h"

namespace ara
{
    namespace crypto
    {
        static const core::InstanceSpecifier cSpecifier("CryptoTest");

        TEST(CryptoServiceProviderTest, LoadCryptoProviderSucceeds)
        {
            auto _result = LoadCryptoProvider(cSpecifier);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().GetInstanceId(), "CryptoTest");
        }

        TEST(CryptoServiceProviderTest, ComputeDigestDelegates)
        {
            auto _provResult = LoadCryptoProvider(cSpecifier);
            ASSERT_TRUE(_provResult.HasValue());
            const auto &_provider = _provResult.Value();

            std::vector<std::uint8_t> _data{0x61, 0x62, 0x63}; // "abc"
            auto _digest = _provider.ComputeDigest(_data, DigestAlgorithm::kSha256);
            ASSERT_TRUE(_digest.HasValue());
            EXPECT_EQ(_digest.Value().size(), 32U); // SHA-256 = 32 bytes
        }

        TEST(CryptoServiceProviderTest, GenerateRandomBytesDelegates)
        {
            auto _provResult = LoadCryptoProvider(cSpecifier);
            ASSERT_TRUE(_provResult.HasValue());

            auto _rng = _provResult.Value().GenerateRandomBytes(16);
            ASSERT_TRUE(_rng.HasValue());
            EXPECT_EQ(_rng.Value().size(), 16U);
        }

        TEST(CryptoServiceProviderTest, GenerateSymmetricKeyDelegates)
        {
            auto _provResult = LoadCryptoProvider(cSpecifier);
            ASSERT_TRUE(_provResult.HasValue());

            auto _key = _provResult.Value().GenerateSymmetricKey(32);
            ASSERT_TRUE(_key.HasValue());
            EXPECT_EQ(_key.Value().size(), 32U);
        }

        TEST(CryptoServiceProviderTest, AesEncryptDecryptRoundTrip)
        {
            auto _provResult = LoadCryptoProvider(cSpecifier);
            ASSERT_TRUE(_provResult.HasValue());
            auto &_provider = _provResult.Value();

            auto _keyResult = _provider.GenerateSymmetricKey(16);
            ASSERT_TRUE(_keyResult.HasValue());
            auto &_key = _keyResult.Value();

            auto _ivResult = _provider.GenerateRandomBytes(16);
            ASSERT_TRUE(_ivResult.HasValue());
            auto &_iv = _ivResult.Value();

            std::vector<std::uint8_t> _plaintext{1, 2, 3, 4, 5};
            auto _enc = _provider.AesEncrypt(_plaintext, _key, _iv);
            ASSERT_TRUE(_enc.HasValue());

            auto _dec = _provider.AesDecrypt(_enc.Value(), _key, _iv);
            ASSERT_TRUE(_dec.HasValue());
            EXPECT_EQ(_dec.Value(), _plaintext);
        }
    }
}
