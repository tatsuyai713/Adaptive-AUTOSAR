#include <gtest/gtest.h>
#include "../../../src/ara/com/instance_identifier.h"

namespace ara
{
    namespace com
    {
        TEST(InstanceIdentifierTest, ConstructFromString)
        {
            InstanceIdentifier id("someip:1234:1");
            EXPECT_EQ(id.ToString(), "someip:1234:1");
        }

        TEST(InstanceIdentifierTest, ConstructFromNumericIds)
        {
            InstanceIdentifier id(0x1234, 0x0001);
            EXPECT_EQ(id.ToString(), "someip:4660:1");
        }

        TEST(InstanceIdentifierTest, Equality)
        {
            InstanceIdentifier a("test:abc");
            InstanceIdentifier b("test:abc");
            InstanceIdentifier c("test:xyz");

            EXPECT_TRUE(a == b);
            EXPECT_FALSE(a == c);
            EXPECT_TRUE(a != c);
        }

        TEST(InstanceIdentifierTest, LessThan)
        {
            InstanceIdentifier a("aaa");
            InstanceIdentifier b("bbb");

            EXPECT_TRUE(a < b);
            EXPECT_FALSE(b < a);
        }

        TEST(InstanceIdentifierTest, HashFunction)
        {
            InstanceIdentifier a("test:abc");
            InstanceIdentifier b("test:abc");
            InstanceIdentifier c("test:xyz");

            std::hash<InstanceIdentifier> hasher;
            EXPECT_EQ(hasher(a), hasher(b));
            // Not guaranteed to differ, but very likely
            // EXPECT_NE(hasher(a), hasher(c));
        }

        TEST(InstanceIdentifierTest, Resolve)
        {
            core::InstanceSpecifier specifier("/app/port");
            auto result = InstanceIdentifier::Resolve(specifier);
            ASSERT_TRUE(result.HasValue());
            EXPECT_FALSE(result.Value().empty());
            EXPECT_EQ(result.Value()[0].ToString(), "/app/port");
        }
    }
}
