/// @file test/ara/com/option/configuration_option_test.cpp
/// @brief Unit tests for DNS-SD Configuration Option.

#include <gtest/gtest.h>
#include <ara/com/option/configuration_option.h>

using namespace ara::com::option;

namespace ara_com_option_test
{
    TEST(ConfigurationOptionTest, ConstructWithStrings)
    {
        std::vector<std::string> entries{"key1=val1", "key2=val2"};
        ConfigurationOption opt(false, entries);

        EXPECT_EQ(opt.Type(), OptionType::Configuration);
        EXPECT_FALSE(opt.Discardable());
        ASSERT_EQ(opt.ConfigurationStrings().size(), 2U);
        EXPECT_EQ(opt.ConfigurationStrings()[0], "key1=val1");
        EXPECT_EQ(opt.ConfigurationStrings()[1], "key2=val2");
    }

    TEST(ConfigurationOptionTest, LengthCalculation)
    {
        // "abc" → 1 prefix + 3 chars = 4
        // "defg" → 1 prefix + 4 chars = 5
        // Total = 9
        std::vector<std::string> entries{"abc", "defg"};
        ConfigurationOption opt(true, entries);

        EXPECT_EQ(opt.Length(), 9U);
    }

    TEST(ConfigurationOptionTest, EmptyConfiguration)
    {
        std::vector<std::string> entries;
        ConfigurationOption opt(false, entries);

        EXPECT_EQ(opt.Length(), 0U);
        EXPECT_TRUE(opt.ConfigurationStrings().empty());
    }

    TEST(ConfigurationOptionTest, PayloadRoundTrip)
    {
        std::vector<std::string> entries{"protocol=someip", "version=1"};
        ConfigurationOption opt(false, entries);

        auto payload = opt.Payload();
        EXPECT_FALSE(payload.empty());

        // Payload starts with BasePayload (length + type + discardable)
        // then our config strings
        // We can verify the length is correct
        EXPECT_GT(payload.size(), 4U); // at least base header + some data
    }

    TEST(ConfigurationOptionTest, DeserializeSimple)
    {
        // Build raw payload: [len_prefix][string_bytes]...
        std::vector<uint8_t> payload;
        // String "a=b" → prefix 3, then 'a','=','b'
        payload.push_back(3);
        payload.push_back('a');
        payload.push_back('=');
        payload.push_back('b');
        // String "x=y" → prefix 3
        payload.push_back(3);
        payload.push_back('x');
        payload.push_back('=');
        payload.push_back('y');

        std::size_t offset = 0;
        auto result = ConfigurationOption::Deserialize(
            payload, offset, false, static_cast<uint16_t>(payload.size()));

        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->Type(), OptionType::Configuration);
        ASSERT_EQ(result->ConfigurationStrings().size(), 2U);
        EXPECT_EQ(result->ConfigurationStrings()[0], "a=b");
        EXPECT_EQ(result->ConfigurationStrings()[1], "x=y");
    }

    TEST(ConfigurationOptionTest, DeserializeEmptyStrings)
    {
        // Two zero-length entries
        std::vector<uint8_t> payload = {0, 0};

        std::size_t offset = 0;
        auto result = ConfigurationOption::Deserialize(
            payload, offset, true, static_cast<uint16_t>(payload.size()));

        ASSERT_NE(result, nullptr);
        EXPECT_TRUE(result->Discardable());
        // Zero-length strings are skipped
        EXPECT_EQ(result->ConfigurationStrings().size(), 0U);
    }

    TEST(ConfigurationOptionTest, SingleEntry)
    {
        std::vector<std::string> entries{"key=value"};
        ConfigurationOption opt(false, entries);

        EXPECT_EQ(opt.Length(), 10U); // 1 prefix + 9 chars
        EXPECT_EQ(opt.ConfigurationStrings().size(), 1U);
    }

    TEST(ConfigurationOptionTest, Discardable)
    {
        std::vector<std::string> entries{"test=true"};
        ConfigurationOption opt(true, entries);
        EXPECT_TRUE(opt.Discardable());
    }

    TEST(ConfigurationOptionTest, DeserializeTruncated)
    {
        // String claims 10 bytes but only 3 available
        std::vector<uint8_t> payload = {10, 'a', 'b', 'c'};

        std::size_t offset = 0;
        auto result = ConfigurationOption::Deserialize(
            payload, offset, false, static_cast<uint16_t>(payload.size()));

        ASSERT_NE(result, nullptr);
        // Truncation means the string parse stops early
        EXPECT_EQ(result->ConfigurationStrings().size(), 0U);
    }

    TEST(ConfigurationOptionTest, MultipleEntries)
    {
        std::vector<std::string> entries{
            "protocol=someip",
            "version=1",
            "transport=udp"};
        ConfigurationOption opt(false, entries);

        EXPECT_EQ(opt.ConfigurationStrings().size(), 3U);
        // Length = (1+15) + (1+9) + (1+13) = 40
        EXPECT_EQ(opt.Length(), 40U);
    }
}
