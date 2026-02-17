#include <gtest/gtest.h>
#include "../../../src/ara/iam/policy_version.h"

namespace ara
{
    namespace iam
    {
        TEST(PolicyVersionManagerTest, CreateSnapshotIncrementsVersion)
        {
            AccessControl _ac;
            _ac.SetPolicy("app1", "svc1", "read", PermissionDecision::kAllow);

            PolicyVersionManager _pvm;
            auto _r1 = _pvm.CreateSnapshot(_ac, "initial", 1000U);
            ASSERT_TRUE(_r1.HasValue());
            EXPECT_EQ(_r1.Value(), 1U);

            auto _r2 = _pvm.CreateSnapshot(_ac, "second", 2000U);
            ASSERT_TRUE(_r2.HasValue());
            EXPECT_EQ(_r2.Value(), 2U);
        }

        TEST(PolicyVersionManagerTest, GetCurrentVersion)
        {
            AccessControl _ac;
            _ac.SetPolicy("app1", "svc1", "read", PermissionDecision::kAllow);

            PolicyVersionManager _pvm;
            EXPECT_EQ(_pvm.GetCurrentVersion(), 0U);

            _pvm.CreateSnapshot(_ac, "v1", 1000U);
            EXPECT_EQ(_pvm.GetCurrentVersion(), 1U);
        }

        TEST(PolicyVersionManagerTest, ListVersions)
        {
            AccessControl _ac;
            _ac.SetPolicy("app1", "svc1", "read", PermissionDecision::kAllow);

            PolicyVersionManager _pvm;
            _pvm.CreateSnapshot(_ac, "v1", 1000U);
            _pvm.CreateSnapshot(_ac, "v2", 2000U);

            auto _versions = _pvm.ListVersions();
            EXPECT_EQ(_versions.size(), 2U);
        }

        TEST(PolicyVersionManagerTest, GetSnapshotValid)
        {
            AccessControl _ac;
            _ac.SetPolicy("app1", "svc1", "read", PermissionDecision::kAllow);

            PolicyVersionManager _pvm;
            _pvm.CreateSnapshot(_ac, "first", 1000U);

            auto _result = _pvm.GetSnapshot(1U);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().Version, 1U);
            EXPECT_EQ(_result.Value().Description, "first");
        }

        TEST(PolicyVersionManagerTest, GetSnapshotInvalidFails)
        {
            PolicyVersionManager _pvm;
            auto _result = _pvm.GetSnapshot(999U);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(PolicyVersionManagerTest, RestoreSnapshot)
        {
            AccessControl _ac;
            _ac.SetPolicy("app1", "svc1", "read", PermissionDecision::kAllow);

            PolicyVersionManager _pvm;
            _pvm.CreateSnapshot(_ac, "v1", 1000U);

            // Modify policies
            _ac.SetPolicy("app2", "svc2", "write", PermissionDecision::kDeny);
            _pvm.CreateSnapshot(_ac, "v2", 2000U);

            // Restore v1
            AccessControl _acRestored;
            auto _result = _pvm.RestoreSnapshot(1U, _acRestored);
            EXPECT_TRUE(_result.HasValue());

            // Verify restored state has original policy
            auto _allowed = _acRestored.IsAllowed("app1", "svc1", "read");
            ASSERT_TRUE(_allowed.HasValue());
            EXPECT_TRUE(_allowed.Value());
        }

        TEST(PolicyVersionManagerTest, RestoreInvalidVersionFails)
        {
            AccessControl _ac;
            PolicyVersionManager _pvm;

            auto _result = _pvm.RestoreSnapshot(999U, _ac);
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
