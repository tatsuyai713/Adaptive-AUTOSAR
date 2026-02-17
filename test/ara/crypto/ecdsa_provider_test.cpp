#include <gtest/gtest.h>
#include "../../../src/ara/crypto/ecdsa_provider.h"

namespace ara
{
    namespace crypto
    {
        TEST(EcdsaProviderTest, GenerateP256KeyPair)
        {
            auto _result = GenerateEcKeyPair(EllipticCurve::kP256);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value().PublicKeyDer.empty());
            EXPECT_FALSE(_result.Value().PrivateKeyDer.empty());
        }

        TEST(EcdsaProviderTest, GenerateP384KeyPair)
        {
            auto _result = GenerateEcKeyPair(EllipticCurve::kP384);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value().PublicKeyDer.empty());
        }

        TEST(EcdsaProviderTest, SignAndVerifyP256)
        {
            auto _keyResult = GenerateEcKeyPair(EllipticCurve::kP256);
            ASSERT_TRUE(_keyResult.HasValue());

            std::vector<std::uint8_t> _data{
                0x48, 0x65, 0x6C, 0x6C, 0x6F};

            auto _sigResult = EcdsaSign(
                _data, _keyResult.Value().PrivateKeyDer);
            ASSERT_TRUE(_sigResult.HasValue());
            EXPECT_FALSE(_sigResult.Value().empty());

            auto _verifyResult = EcdsaVerify(
                _data, _sigResult.Value(),
                _keyResult.Value().PublicKeyDer);
            ASSERT_TRUE(_verifyResult.HasValue());
            EXPECT_TRUE(_verifyResult.Value());
        }

        TEST(EcdsaProviderTest, SignAndVerifyP384)
        {
            auto _keyResult = GenerateEcKeyPair(EllipticCurve::kP384);
            ASSERT_TRUE(_keyResult.HasValue());

            std::vector<std::uint8_t> _data{0x01, 0x02, 0x03, 0x04};

            auto _sigResult = EcdsaSign(
                _data, _keyResult.Value().PrivateKeyDer,
                DigestAlgorithm::kSha384);
            ASSERT_TRUE(_sigResult.HasValue());

            auto _verifyResult = EcdsaVerify(
                _data, _sigResult.Value(),
                _keyResult.Value().PublicKeyDer,
                DigestAlgorithm::kSha384);
            ASSERT_TRUE(_verifyResult.HasValue());
            EXPECT_TRUE(_verifyResult.Value());
        }

        TEST(EcdsaProviderTest, VerifyWithWrongKeyFails)
        {
            auto _key1 = GenerateEcKeyPair(EllipticCurve::kP256);
            auto _key2 = GenerateEcKeyPair(EllipticCurve::kP256);
            ASSERT_TRUE(_key1.HasValue());
            ASSERT_TRUE(_key2.HasValue());

            std::vector<std::uint8_t> _data{0xAA, 0xBB};
            auto _sig = EcdsaSign(_data, _key1.Value().PrivateKeyDer);
            ASSERT_TRUE(_sig.HasValue());

            auto _result = EcdsaVerify(
                _data, _sig.Value(), _key2.Value().PublicKeyDer);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value());
        }

        TEST(EcdsaProviderTest, InvalidKeyFormatFails)
        {
            std::vector<std::uint8_t> _badKey{0x00, 0x01};
            std::vector<std::uint8_t> _data{0x01};

            auto _result = EcdsaSign(_data, _badKey);
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
