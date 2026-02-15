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
    }
}
