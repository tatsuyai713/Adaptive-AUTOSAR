#include <cstdio>
#include <fstream>
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

        TEST(AccessControlTest, SaveAndLoadRoundTrip)
        {
            const std::string cFilePath{"/tmp/iam_test_policies.txt"};

            {
                AccessControl _accessControl;
                _accessControl.SetPolicy(
                    "app_a", "svc_x", "read", PermissionDecision::kAllow);
                _accessControl.SetPolicy(
                    "app_b", "svc_y", "write", PermissionDecision::kDeny);

                auto _saveResult = _accessControl.SaveToFile(cFilePath);
                ASSERT_TRUE(_saveResult.HasValue());
            }

            {
                AccessControl _loaded;
                auto _loadResult = _loaded.LoadFromFile(cFilePath);
                ASSERT_TRUE(_loadResult.HasValue());

                auto _decision1 = _loaded.IsAllowed("app_a", "svc_x", "read");
                ASSERT_TRUE(_decision1.HasValue());
                EXPECT_TRUE(_decision1.Value());

                auto _decision2 = _loaded.IsAllowed("app_b", "svc_y", "write");
                ASSERT_TRUE(_decision2.HasValue());
                EXPECT_FALSE(_decision2.Value());
            }

            std::remove(cFilePath.c_str());
        }

        TEST(AccessControlTest, LoadFromNonExistentFileReturnsError)
        {
            AccessControl _accessControl;
            auto _result = _accessControl.LoadFromFile("/tmp/iam_no_such_file.txt");
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<IamErrc>(_result.Error().Value()),
                IamErrc::kPolicyStoreError);
        }

        TEST(AccessControlTest, LoadFromMalformedFileReturnsParseError)
        {
            const std::string cFilePath{"/tmp/iam_test_malformed.txt"};

            {
                std::ofstream _file{cFilePath};
                _file << "bad_line_no_pipes\n";
            }

            AccessControl _accessControl;
            auto _result = _accessControl.LoadFromFile(cFilePath);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<IamErrc>(_result.Error().Value()),
                IamErrc::kPolicyFileParseError);

            std::remove(cFilePath.c_str());
        }

        TEST(AccessControlTest, LoadFromInvalidDecisionReturnsParseError)
        {
            const std::string cFilePath{"/tmp/iam_test_bad_decision.txt"};

            {
                std::ofstream _file{cFilePath};
                _file << "app|svc|read|maybe\n";
            }

            AccessControl _accessControl;
            auto _result = _accessControl.LoadFromFile(cFilePath);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_EQ(
                static_cast<IamErrc>(_result.Error().Value()),
                IamErrc::kPolicyFileParseError);

            std::remove(cFilePath.c_str());
        }

        TEST(AccessControlTest, AuditCallbackInvoked)
        {
            AccessControl _accessControl;
            _accessControl.SetPolicy(
                "app_z", "svc_w", "execute", PermissionDecision::kAllow);

            std::string _auditSubject;
            std::string _auditResource;
            std::string _auditAction;
            bool _auditAllowed{false};

            _accessControl.SetAuditCallback(
                [&](const std::string &subject,
                    const std::string &resource,
                    const std::string &action,
                    bool allowed)
                {
                    _auditSubject = subject;
                    _auditResource = resource;
                    _auditAction = action;
                    _auditAllowed = allowed;
                });

            auto _decision = _accessControl.IsAllowed("app_z", "svc_w", "execute");
            ASSERT_TRUE(_decision.HasValue());
            EXPECT_TRUE(_decision.Value());

            EXPECT_EQ(_auditSubject, "app_z");
            EXPECT_EQ(_auditResource, "svc_w");
            EXPECT_EQ(_auditAction, "execute");
            EXPECT_TRUE(_auditAllowed);
        }

        TEST(AccessControlTest, AuditCallbackReportsDeny)
        {
            AccessControl _accessControl;

            bool _auditAllowed{true};
            _accessControl.SetAuditCallback(
                [&](const std::string &,
                    const std::string &,
                    const std::string &,
                    bool allowed)
                {
                    _auditAllowed = allowed;
                });

            auto _decision = _accessControl.IsAllowed("unknown", "svc", "read");
            ASSERT_TRUE(_decision.HasValue());
            EXPECT_FALSE(_decision.Value());
            EXPECT_FALSE(_auditAllowed);
        }
    }
}
