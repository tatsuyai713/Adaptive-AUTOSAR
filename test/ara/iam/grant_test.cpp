#include <gtest/gtest.h>
#include "../../../src/ara/iam/grant.h"

namespace ara
{
    namespace iam
    {
        TEST(GrantTest, ConstructAndInfo)
        {
            Grant _grant{"g1", "app1", "svc1", "read", 1000U, 2000U};
            const auto &_info = _grant.Info();
            EXPECT_EQ(_info.GrantId, "g1");
            EXPECT_EQ(_info.Subject, "app1");
            EXPECT_EQ(_info.Resource, "svc1");
            EXPECT_EQ(_info.Action, "read");
            EXPECT_EQ(_info.IssuedAtEpochMs, 1000U);
            EXPECT_EQ(_info.ExpiresAtEpochMs, 2000U);
            EXPECT_FALSE(_info.Revoked);
        }

        TEST(GrantTest, IsValidBeforeExpiry)
        {
            Grant _grant{"g1", "app1", "svc1", "read", 1000U, 5000U};
            EXPECT_TRUE(_grant.IsValid(1500U));
            EXPECT_TRUE(_grant.IsValid(4999U));
        }

        TEST(GrantTest, IsInvalidAfterExpiry)
        {
            Grant _grant{"g1", "app1", "svc1", "read", 1000U, 5000U};
            EXPECT_FALSE(_grant.IsValid(5000U));
            EXPECT_FALSE(_grant.IsValid(9999U));
        }

        TEST(GrantTest, NoExpiryAlwaysValid)
        {
            Grant _grant{"g1", "app1", "svc1", "read", 1000U, 0U};
            EXPECT_TRUE(_grant.IsValid(0U));
            EXPECT_TRUE(_grant.IsValid(999999999U));
        }

        TEST(GrantTest, RevokeInvalidates)
        {
            Grant _grant{"g1", "app1", "svc1", "read", 1000U, 5000U};
            EXPECT_TRUE(_grant.IsValid(1500U));
            _grant.Revoke();
            EXPECT_FALSE(_grant.IsValid(1500U));
            EXPECT_TRUE(_grant.Info().Revoked);
        }
    }
}
