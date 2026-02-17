#include <gtest/gtest.h>
#include "../../../src/ara/iam/grant_manager.h"

namespace ara
{
    namespace iam
    {
        TEST(GrantManagerTest, IssueGrantReturnsUniqueId)
        {
            GrantManager _mgr;
            auto _r1 = _mgr.IssueGrant("app1", "svc1", "read", 5000U, 1000U);
            auto _r2 = _mgr.IssueGrant("app1", "svc1", "write", 5000U, 1000U);
            ASSERT_TRUE(_r1.HasValue());
            ASSERT_TRUE(_r2.HasValue());
            EXPECT_NE(_r1.Value(), _r2.Value());
        }

        TEST(GrantManagerTest, IssueWithEmptyFieldsFails)
        {
            GrantManager _mgr;
            auto _r = _mgr.IssueGrant("", "svc1", "read", 5000U, 1000U);
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(GrantManagerTest, IsGrantValidBeforeAndAfterExpiry)
        {
            GrantManager _mgr;
            auto _r = _mgr.IssueGrant("app1", "svc1", "read", 5000U, 1000U);
            ASSERT_TRUE(_r.HasValue());

            auto _valid = _mgr.IsGrantValid(_r.Value(), 3000U);
            ASSERT_TRUE(_valid.HasValue());
            EXPECT_TRUE(_valid.Value());

            auto _expired = _mgr.IsGrantValid(_r.Value(), 6001U);
            ASSERT_TRUE(_expired.HasValue());
            EXPECT_FALSE(_expired.Value());
        }

        TEST(GrantManagerTest, RevokeGrant)
        {
            GrantManager _mgr;
            auto _r = _mgr.IssueGrant("app1", "svc1", "read", 0U, 1000U);
            ASSERT_TRUE(_r.HasValue());

            auto _revokeResult = _mgr.RevokeGrant(_r.Value());
            EXPECT_TRUE(_revokeResult.HasValue());

            auto _valid = _mgr.IsGrantValid(_r.Value(), 1500U);
            ASSERT_TRUE(_valid.HasValue());
            EXPECT_FALSE(_valid.Value());
        }

        TEST(GrantManagerTest, RevokeNonexistentFails)
        {
            GrantManager _mgr;
            auto _r = _mgr.RevokeGrant("nonexistent");
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(GrantManagerTest, GetGrantsForSubject)
        {
            GrantManager _mgr;
            _mgr.IssueGrant("app1", "svc1", "read", 0U, 1000U);
            _mgr.IssueGrant("app1", "svc2", "write", 0U, 1000U);
            _mgr.IssueGrant("app2", "svc1", "read", 0U, 1000U);

            auto _grants = _mgr.GetGrantsForSubject("app1");
            EXPECT_EQ(_grants.size(), 2U);
        }

        TEST(GrantManagerTest, PurgeExpiredRemovesOld)
        {
            GrantManager _mgr;
            _mgr.IssueGrant("app1", "svc1", "read", 1000U, 100U);
            _mgr.IssueGrant("app1", "svc2", "write", 0U, 100U);

            _mgr.PurgeExpired(2000U);

            auto _grants = _mgr.GetGrantsForSubject("app1");
            EXPECT_EQ(_grants.size(), 1U);
            EXPECT_EQ(_grants[0].Resource, "svc2");
        }

        TEST(GrantManagerTest, SaveAndLoadRoundTrip)
        {
            const std::string _path{"/tmp/autosar_test_grants.csv"};

            GrantManager _mgr;
            _mgr.IssueGrant("app1", "svc1", "read", 5000U, 1000U);
            _mgr.IssueGrant("app2", "svc2", "write", 0U, 2000U);

            auto _saveResult = _mgr.SaveToFile(_path);
            EXPECT_TRUE(_saveResult.HasValue());

            GrantManager _mgr2;
            auto _loadResult = _mgr2.LoadFromFile(_path);
            EXPECT_TRUE(_loadResult.HasValue());

            auto _grants = _mgr2.GetGrantsForSubject("app1");
            EXPECT_EQ(_grants.size(), 1U);

            auto _grants2 = _mgr2.GetGrantsForSubject("app2");
            EXPECT_EQ(_grants2.size(), 1U);
        }
    }
}
