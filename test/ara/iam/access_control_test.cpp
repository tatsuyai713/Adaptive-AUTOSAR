#include <gtest/gtest.h>
#include "../../../src/ara/iam/access_control.h"

namespace ara
{
    namespace iam
    {
        TEST(AccessControlTest, SetPolicyAndAllowExactMatch)
        {
            AccessControl _accessControl;
            ASSERT_TRUE(
                _accessControl.SetPolicy(
                    "powertrain_app",
                    "vehicle_speed_service",
                    "read",
                    PermissionDecision::kAllow)
                    .HasValue());

            const auto _decision = _accessControl.IsAllowed(
                "powertrain_app",
                "vehicle_speed_service",
                "read");
            ASSERT_TRUE(_decision.HasValue());
            EXPECT_TRUE(_decision.Value());
        }

        TEST(AccessControlTest, DefaultDenyWhenPolicyMissing)
        {
            AccessControl _accessControl;
            const auto _decision = _accessControl.IsAllowed(
                "diagnostic_app",
                "vehicle_speed_service",
                "write");

            ASSERT_TRUE(_decision.HasValue());
            EXPECT_FALSE(_decision.Value());
        }

        TEST(AccessControlTest, WildcardPolicyApplies)
        {
            AccessControl _accessControl;
            ASSERT_TRUE(
                _accessControl.SetPolicy(
                    "diagnostic_app",
                    "*",
                    "read",
                    PermissionDecision::kAllow)
                    .HasValue());

            const auto _decision = _accessControl.IsAllowed(
                "diagnostic_app",
                "brake_status_service",
                "read");
            ASSERT_TRUE(_decision.HasValue());
            EXPECT_TRUE(_decision.Value());
        }

        TEST(AccessControlTest, EmptyArgumentReturnsError)
        {
            AccessControl _accessControl;
            const auto _result = _accessControl.SetPolicy(
                "",
                "vehicle_speed_service",
                "read",
                PermissionDecision::kAllow);

            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Iam");
        }

        TEST(AccessControlTest, SpecificPolicyOverridesWildcardFallbackByOrder)
        {
            AccessControl _accessControl;
            ASSERT_TRUE(
                _accessControl.SetPolicy(
                    "*",
                    "*",
                    "read",
                    PermissionDecision::kDeny)
                    .HasValue());
            ASSERT_TRUE(
                _accessControl.SetPolicy(
                    "safety_app",
                    "steering_service",
                    "read",
                    PermissionDecision::kAllow)
                    .HasValue());

            const auto _decision = _accessControl.IsAllowed(
                "safety_app",
                "steering_service",
                "read");
            ASSERT_TRUE(_decision.HasValue());
            EXPECT_TRUE(_decision.Value());
        }
    }
}
