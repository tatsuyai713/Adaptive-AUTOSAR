#include <gtest/gtest.h>
#include "../../../src/ara/core/ap_release_info.h"

namespace ara
{
    namespace core
    {
        TEST(ApReleaseInfoTest, DefaultReleaseMatchesR2411)
        {
            EXPECT_STREQ(ApReleaseInfo::cReleaseString, "R24-11");
            EXPECT_EQ(ApReleaseInfo::cReleaseYear, 24U);
            EXPECT_EQ(ApReleaseInfo::cReleaseMonth, 11U);
            EXPECT_EQ(ApReleaseInfo::cReleaseCompact, 2411U);
        }

        TEST(ApReleaseInfoTest, IsAtLeastWorksForOlderAndEqualProfiles)
        {
            EXPECT_TRUE(ApReleaseInfo::IsAtLeast(22U, 11U));
            EXPECT_TRUE(ApReleaseInfo::IsAtLeast(24U, 11U));
            EXPECT_FALSE(ApReleaseInfo::IsAtLeast(24U, 12U));
            EXPECT_FALSE(ApReleaseInfo::IsAtLeast(25U, 1U));
        }
    }
}
