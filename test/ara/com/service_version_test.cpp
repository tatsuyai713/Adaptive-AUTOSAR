#include <gtest/gtest.h>
#include "../../../src/ara/com/service_version.h"

namespace ara
{
    namespace com
    {
        TEST(ServiceVersionTest, DefaultVersion)
        {
            ServiceVersion v;
            EXPECT_EQ(v.MajorVersion, 1U);
            EXPECT_EQ(v.MinorVersion, 0U);
        }

        TEST(ServiceVersionTest, CustomVersion)
        {
            ServiceVersion v(2, 5);
            EXPECT_EQ(v.MajorVersion, 2U);
            EXPECT_EQ(v.MinorVersion, 5U);
        }

        TEST(ServiceVersionTest, ToString)
        {
            ServiceVersion v(3, 14);
            EXPECT_EQ(v.ToString(), "3.14");
        }

        TEST(ServiceVersionTest, Equality)
        {
            ServiceVersion a(1, 0);
            ServiceVersion b(1, 0);
            ServiceVersion c(1, 1);

            EXPECT_TRUE(a == b);
            EXPECT_FALSE(a == c);
            EXPECT_TRUE(a != c);
        }

        TEST(ServiceVersionTest, LessThan)
        {
            ServiceVersion a(1, 0);
            ServiceVersion b(1, 1);
            ServiceVersion c(2, 0);

            EXPECT_TRUE(a < b);
            EXPECT_TRUE(b < c);
            EXPECT_FALSE(c < a);
        }

        TEST(ServiceVersionTest, ExactCompatibility)
        {
            ServiceVersion proxy(1, 5);
            ServiceVersion skeleton_exact(1, 5);
            ServiceVersion skeleton_minor_higher(1, 6);
            ServiceVersion skeleton_major_diff(2, 5);

            EXPECT_TRUE(proxy.IsCompatibleWith(
                skeleton_exact, VersionCheckPolicy::kExact));
            EXPECT_FALSE(proxy.IsCompatibleWith(
                skeleton_minor_higher, VersionCheckPolicy::kExact));
            EXPECT_FALSE(proxy.IsCompatibleWith(
                skeleton_major_diff, VersionCheckPolicy::kExact));
        }

        TEST(ServiceVersionTest, MajorOnlyCompatibility)
        {
            ServiceVersion proxy(1, 5);
            ServiceVersion same_major(1, 0);
            ServiceVersion diff_major(2, 5);

            EXPECT_TRUE(proxy.IsCompatibleWith(
                same_major, VersionCheckPolicy::kMajorOnly));
            EXPECT_FALSE(proxy.IsCompatibleWith(
                diff_major, VersionCheckPolicy::kMajorOnly));
        }

        TEST(ServiceVersionTest, MinorBackwardCompatibility)
        {
            ServiceVersion proxy(1, 3);
            ServiceVersion skeleton_higher(1, 5);
            ServiceVersion skeleton_same(1, 3);
            ServiceVersion skeleton_lower(1, 2);
            ServiceVersion skeleton_diff_major(2, 5);

            EXPECT_TRUE(proxy.IsCompatibleWith(
                skeleton_higher, VersionCheckPolicy::kMinorBackward));
            EXPECT_TRUE(proxy.IsCompatibleWith(
                skeleton_same, VersionCheckPolicy::kMinorBackward));
            EXPECT_FALSE(proxy.IsCompatibleWith(
                skeleton_lower, VersionCheckPolicy::kMinorBackward));
            EXPECT_FALSE(proxy.IsCompatibleWith(
                skeleton_diff_major, VersionCheckPolicy::kMinorBackward));
        }

        TEST(ServiceVersionTest, NoCheckAlwaysCompatible)
        {
            ServiceVersion a(1, 0);
            ServiceVersion b(99, 99);

            EXPECT_TRUE(a.IsCompatibleWith(
                b, VersionCheckPolicy::kNoCheck));
        }

        TEST(ServiceVersionTest, DefaultPolicyIsMinorBackward)
        {
            ServiceVersion proxy(1, 3);
            ServiceVersion skeleton(1, 5);

            // Default policy should be kMinorBackward
            EXPECT_TRUE(proxy.IsCompatibleWith(skeleton));
        }
    }
}
