#include <gtest/gtest.h>
#include <iomanip>
#include <sstream>
#include "../../../src/ara/crypto/crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        namespace
        {
            std::string BytesToHex(const std::vector<std::uint8_t> &bytes)
            {
                std::ostringstream _stream;
                for (const auto _value : bytes)
                {
                    _stream << std::hex << std::setfill('0') << std::setw(2)
                            << static_cast<int>(_value);
                }

                return _stream.str();
            }
        }

        TEST(CryptoProviderTest, ComputeSha256ForAbc)
        {
            const std::vector<std::uint8_t> cInput{'a', 'b', 'c'};
            const auto _result = ComputeDigest(cInput, DigestAlgorithm::kSha256);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        }

        TEST(CryptoProviderTest, ComputeSha256ForEmptyPayload)
        {
            const std::vector<std::uint8_t> cInput;
            const auto _result = ComputeDigest(cInput, DigestAlgorithm::kSha256);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        }

        TEST(CryptoProviderTest, UnsupportedAlgorithmReturnsError)
        {
            const std::vector<std::uint8_t> cInput{'x'};
            const auto _result = ComputeDigest(
                cInput,
                static_cast<DigestAlgorithm>(999U));

            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(
                _result.Error().Domain().Name(),
                "Crypto");
        }

        TEST(CryptoProviderTest, GenerateRandomBytesReturnsRequestedSize)
        {
            const auto _result = GenerateRandomBytes(64U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().size(), 64U);
        }

        TEST(CryptoProviderTest, GenerateZeroRandomBytesReturnsEmptyVector)
        {
            const auto _result = GenerateRandomBytes(0U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_TRUE(_result.Value().empty());
        }

        TEST(CryptoProviderTest, ComputeSha1ForAbc)
        {
            const std::vector<std::uint8_t> cInput{'a', 'b', 'c'};
            const auto _result = ComputeDigest(cInput, DigestAlgorithm::kSha1);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "a9993e364706816aba3e25717850c26c9cd0d89d");
        }

        TEST(CryptoProviderTest, ComputeSha384ForAbc)
        {
            const std::vector<std::uint8_t> cInput{'a', 'b', 'c'};
            const auto _result = ComputeDigest(cInput, DigestAlgorithm::kSha384);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded163"
                "1a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
        }

        TEST(CryptoProviderTest, ComputeSha512ForAbc)
        {
            const std::vector<std::uint8_t> cInput{'a', 'b', 'c'};
            const auto _result = ComputeDigest(cInput, DigestAlgorithm::kSha512);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea2"
                "0a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd"
                "454d4423643ce80e2a9ac94fa54ca49f");
        }

        TEST(CryptoProviderTest, ComputeHmacSha256KnownVector)
        {
            // RFC 4231 Test Case 2
            const std::vector<std::uint8_t> cKey{'J', 'e', 'f', 'e'};
            const std::string cMsg{"what do ya want for nothing?"};
            const std::vector<std::uint8_t> cData(cMsg.begin(), cMsg.end());

            const auto _result = ComputeHmac(cData, cKey, DigestAlgorithm::kSha256);

            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(
                BytesToHex(_result.Value()),
                "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
        }

        TEST(CryptoProviderTest, ComputeHmacEmptyKeyReturnsError)
        {
            const std::vector<std::uint8_t> cData{'a', 'b', 'c'};
            const std::vector<std::uint8_t> cEmptyKey;

            const auto _result = ComputeHmac(cData, cEmptyKey);

            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kInvalidArgument);
        }

        TEST(CryptoProviderTest, ComputeHmacUnsupportedAlgorithm)
        {
            const std::vector<std::uint8_t> cData{'a'};
            const std::vector<std::uint8_t> cKey{'k'};

            const auto _result = ComputeHmac(
                cData, cKey, static_cast<DigestAlgorithm>(999U));

            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kUnsupportedAlgorithm);
        }

        TEST(CryptoProviderTest, GenerateSymmetricKey128)
        {
            const auto _result = GenerateSymmetricKey(16U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().size(), 16U);
        }

        TEST(CryptoProviderTest, GenerateSymmetricKey256)
        {
            const auto _result = GenerateSymmetricKey(32U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().size(), 32U);
        }

        TEST(CryptoProviderTest, GenerateSymmetricKeyInvalidSize)
        {
            const auto _result = GenerateSymmetricKey(24U);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kInvalidKeySize);
        }

        TEST(CryptoProviderTest, AesEncryptDecryptRoundTrip128)
        {
            const std::vector<std::uint8_t> cPlaintext{
                'H', 'e', 'l', 'l', 'o', ' ', 'A', 'U',
                'T', 'O', 'S', 'A', 'R', '!', '!', '!'};
            const auto _keyResult = GenerateSymmetricKey(16U);
            ASSERT_TRUE(_keyResult.HasValue());
            const auto &_key = _keyResult.Value();

            const auto _ivResult = GenerateRandomBytes(16U);
            ASSERT_TRUE(_ivResult.HasValue());
            const auto &_iv = _ivResult.Value();

            const auto _encResult = AesEncrypt(cPlaintext, _key, _iv);
            ASSERT_TRUE(_encResult.HasValue());
            EXPECT_NE(_encResult.Value(), cPlaintext);

            const auto _decResult = AesDecrypt(_encResult.Value(), _key, _iv);
            ASSERT_TRUE(_decResult.HasValue());
            EXPECT_EQ(_decResult.Value(), cPlaintext);
        }

        TEST(CryptoProviderTest, AesEncryptDecryptRoundTrip256)
        {
            const std::vector<std::uint8_t> cPlaintext{
                'T', 'e', 's', 't', ' ', 'A', 'E', 'S',
                '-', '2', '5', '6', ' ', 'C', 'B', 'C'};
            const auto _keyResult = GenerateSymmetricKey(32U);
            ASSERT_TRUE(_keyResult.HasValue());
            const auto &_key = _keyResult.Value();

            const auto _ivResult = GenerateRandomBytes(16U);
            ASSERT_TRUE(_ivResult.HasValue());
            const auto &_iv = _ivResult.Value();

            const auto _encResult = AesEncrypt(cPlaintext, _key, _iv);
            ASSERT_TRUE(_encResult.HasValue());

            const auto _decResult = AesDecrypt(_encResult.Value(), _key, _iv);
            ASSERT_TRUE(_decResult.HasValue());
            EXPECT_EQ(_decResult.Value(), cPlaintext);
        }

        TEST(CryptoProviderTest, AesEncryptInvalidKeySize)
        {
            const std::vector<std::uint8_t> cPlaintext{'a'};
            const std::vector<std::uint8_t> cBadKey(24U, 0x00);
            const std::vector<std::uint8_t> cIv(16U, 0x00);

            const auto _result = AesEncrypt(cPlaintext, cBadKey, cIv);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kInvalidKeySize);
        }

        TEST(CryptoProviderTest, AesEncryptInvalidIvSize)
        {
            const std::vector<std::uint8_t> cPlaintext{'a'};
            const std::vector<std::uint8_t> cKey(16U, 0x00);
            const std::vector<std::uint8_t> cBadIv(8U, 0x00);

            const auto _result = AesEncrypt(cPlaintext, cKey, cBadIv);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kInvalidArgument);
        }

        TEST(CryptoProviderTest, AesDecryptWrongKeyFails)
        {
            const std::vector<std::uint8_t> cPlaintext{
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            const std::vector<std::uint8_t> cKey(16U, 0x42);
            const std::vector<std::uint8_t> cWrongKey(16U, 0x99);
            const std::vector<std::uint8_t> cIv(16U, 0x00);

            const auto _encResult = AesEncrypt(cPlaintext, cKey, cIv);
            ASSERT_TRUE(_encResult.HasValue());

            const auto _decResult = AesDecrypt(_encResult.Value(), cWrongKey, cIv);
            // Decryption with wrong key should fail (padding error) or produce wrong data
            if (_decResult.HasValue())
            {
                EXPECT_NE(_decResult.Value(), cPlaintext);
            }
            else
            {
                EXPECT_EQ(
                    static_cast<CryptoErrc>(_decResult.Error().Value()),
                    CryptoErrc::kDecryptionFailure);
            }
        }

        TEST(CryptoProviderTest, AesDecryptEmptyCiphertextReturnsError)
        {
            const std::vector<std::uint8_t> cKey(16U, 0x00);
            const std::vector<std::uint8_t> cIv(16U, 0x00);
            const std::vector<std::uint8_t> cEmpty;

            const auto _result = AesDecrypt(cEmpty, cKey, cIv);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<CryptoErrc>(_result.Error().Value()),
                CryptoErrc::kInvalidArgument);
        }
    }
}
