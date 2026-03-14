#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include "../../../../src/ara/com/helper/ipv6_address.h"

namespace ara
{
    namespace com
    {
        namespace helper
        {
            TEST(Ipv6AddressTest, ConstructFromOctets)
            {
                Ipv6Address addr(
                    0xfe, 0x80, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01);

                EXPECT_EQ(addr.Octets[0], 0xfe);
                EXPECT_EQ(addr.Octets[1], 0x80);
                EXPECT_EQ(addr.Octets[15], 0x01);
            }

            TEST(Ipv6AddressTest, ConstructFromArray)
            {
                std::array<uint8_t, 16> octets = {
                    0x20, 0x01, 0x0d, 0xb8,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01};
                Ipv6Address addr(octets);

                EXPECT_EQ(addr.Octets[0], 0x20);
                EXPECT_EQ(addr.Octets[1], 0x01);
                EXPECT_EQ(addr.Octets[15], 0x01);
            }

            TEST(Ipv6AddressTest, ConstructFromFullString)
            {
                Ipv6Address addr("2001:0db8:0000:0000:0000:0000:0000:0001");

                EXPECT_EQ(addr.Octets[0], 0x20);
                EXPECT_EQ(addr.Octets[1], 0x01);
                EXPECT_EQ(addr.Octets[2], 0x0d);
                EXPECT_EQ(addr.Octets[3], 0xb8);
                EXPECT_EQ(addr.Octets[15], 0x01);
            }

            TEST(Ipv6AddressTest, ConstructFromAbbreviatedString)
            {
                Ipv6Address addr("fe80::1");

                EXPECT_EQ(addr.Octets[0], 0xfe);
                EXPECT_EQ(addr.Octets[1], 0x80);
                // Middle octets should be zero
                for (int i = 2; i < 15; ++i)
                {
                    EXPECT_EQ(addr.Octets[i], 0x00);
                }
                EXPECT_EQ(addr.Octets[15], 0x01);
            }

            TEST(Ipv6AddressTest, ConstructLoopback)
            {
                Ipv6Address addr("::1");

                for (int i = 0; i < 15; ++i)
                {
                    EXPECT_EQ(addr.Octets[i], 0x00);
                }
                EXPECT_EQ(addr.Octets[15], 0x01);
            }

            TEST(Ipv6AddressTest, ToStringFullForm)
            {
                Ipv6Address addr(
                    0x20, 0x01, 0x0d, 0xb8,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x01);

                std::string result = addr.ToString();
                EXPECT_EQ(result, "2001:0db8:0000:0000:0000:0000:0000:0001");
            }

            TEST(Ipv6AddressTest, ToStringAllZeros)
            {
                Ipv6Address addr(
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00);

                std::string result = addr.ToString();
                EXPECT_EQ(result, "0000:0000:0000:0000:0000:0000:0000:0000");
            }

            TEST(Ipv6AddressTest, EqualityOperator)
            {
                Ipv6Address addr1("fe80::1");
                Ipv6Address addr2("fe80::1");
                Ipv6Address addr3("fe80::2");

                EXPECT_TRUE(addr1 == addr2);
                EXPECT_FALSE(addr1 == addr3);
            }

            TEST(Ipv6AddressTest, InequalityOperator)
            {
                Ipv6Address addr1("fe80::1");
                Ipv6Address addr2("fe80::2");

                EXPECT_TRUE(addr1 != addr2);
                EXPECT_FALSE(addr1 != addr1);
            }

            TEST(Ipv6AddressTest, InjectAndExtract)
            {
                Ipv6Address original(
                    0x20, 0x01, 0x0d, 0xb8,
                    0x85, 0xa3, 0x00, 0x00,
                    0x00, 0x00, 0x8a, 0x2e,
                    0x03, 0x70, 0x73, 0x34);

                std::vector<uint8_t> buffer;
                Ipv6Address::Inject(buffer, original);

                EXPECT_EQ(buffer.size(), 16U);

                std::size_t offset = 0;
                Ipv6Address extracted = Ipv6Address::Extract(buffer, offset);

                EXPECT_EQ(offset, 16U);
                EXPECT_EQ(original, extracted);
            }

            TEST(Ipv6AddressTest, ExtractWithOffset)
            {
                Ipv6Address original("::1");

                std::vector<uint8_t> buffer = {0xAA, 0xBB}; // prefix
                Ipv6Address::Inject(buffer, original);
                buffer.push_back(0xCC); // suffix

                EXPECT_EQ(buffer.size(), 19U);

                std::size_t offset = 2;
                Ipv6Address extracted = Ipv6Address::Extract(buffer, offset);

                EXPECT_EQ(offset, 18U);
                EXPECT_EQ(original, extracted);
            }
        }
    }
}
