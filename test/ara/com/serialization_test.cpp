#include <gtest/gtest.h>
#include "../../../src/ara/com/serialization.h"

namespace ara
{
    namespace com
    {
        TEST(SerializerTest, IntRoundTrip)
        {
            int original = 42;
            auto bytes = Serializer<int>::Serialize(original);
            EXPECT_EQ(bytes.size(), sizeof(int));

            auto result = Serializer<int>::Deserialize(bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 42);
        }

        TEST(SerializerTest, DoubleRoundTrip)
        {
            double original = 3.14159265358979;
            auto bytes = Serializer<double>::Serialize(original);
            EXPECT_EQ(bytes.size(), sizeof(double));

            auto result = Serializer<double>::Deserialize(bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_DOUBLE_EQ(result.Value(), original);
        }

        struct TestPod
        {
            std::uint32_t Id;
            std::uint16_t Flags;
            std::uint8_t Status;
            std::uint8_t Reserved;
        };

        TEST(SerializerTest, PodStructRoundTrip)
        {
            TestPod original{0x12345678, 0xABCD, 0x01, 0xFF};
            auto bytes = Serializer<TestPod>::Serialize(original);
            EXPECT_EQ(bytes.size(), sizeof(TestPod));

            auto result = Serializer<TestPod>::Deserialize(bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value().Id, 0x12345678);
            EXPECT_EQ(result.Value().Flags, 0xABCD);
            EXPECT_EQ(result.Value().Status, 0x01);
            EXPECT_EQ(result.Value().Reserved, 0xFF);
        }

        TEST(SerializerTest, DeserializeUndersizedBuffer)
        {
            std::vector<std::uint8_t> tooSmall{0x01, 0x02};
            auto result = Serializer<std::uint32_t>::Deserialize(
                tooSmall.data(), tooSmall.size());
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SerializerTest, StringRoundTrip)
        {
            std::string original = "Hello, AUTOSAR AP!";
            auto bytes = Serializer<std::string>::Serialize(original);

            auto result = Serializer<std::string>::Deserialize(
                bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), original);
        }

        TEST(SerializerTest, EmptyStringRoundTrip)
        {
            std::string original;
            auto bytes = Serializer<std::string>::Serialize(original);

            auto result = Serializer<std::string>::Deserialize(
                bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), "");
        }

        TEST(SerializerTest, StringDeserializeUndersized)
        {
            std::vector<std::uint8_t> tooSmall{0x01};
            auto result = Serializer<std::string>::Deserialize(
                tooSmall.data(), tooSmall.size());
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SerializerTest, StringDeserializeTruncatedPayload)
        {
            // Claim length=100 but only provide 4 bytes of header
            std::uint32_t fakeLen = 100;
            std::vector<std::uint8_t> truncated(sizeof(fakeLen));
            std::memcpy(truncated.data(), &fakeLen, sizeof(fakeLen));

            auto result = Serializer<std::string>::Deserialize(
                truncated.data(), truncated.size());
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SerializerTest, RawBytesPassthrough)
        {
            std::vector<std::uint8_t> original{0xDE, 0xAD, 0xBE, 0xEF};
            auto bytes = Serializer<std::vector<std::uint8_t>>::Serialize(original);
            EXPECT_EQ(bytes, original);

            auto result = Serializer<std::vector<std::uint8_t>>::Deserialize(
                bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), original);
        }

        TEST(SerializerTest, Uint8RoundTrip)
        {
            std::uint8_t original = 0xAA;
            auto bytes = Serializer<std::uint8_t>::Serialize(original);
            EXPECT_EQ(bytes.size(), 1U);

            auto result = Serializer<std::uint8_t>::Deserialize(
                bytes.data(), bytes.size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 0xAA);
        }
    }
}
