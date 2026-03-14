#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include "../../../src/ara/iam/abac_policy_engine.h"

namespace ara
{
    namespace iam
    {
        // -----------------------------------------------------------------------
        // Basic rule evaluation
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, EmptyEngineReturnsNotApplicable)
        {
            AbacPolicyEngine engine;
            EXPECT_EQ(
                engine.Evaluate("user", "resource", "read"),
                AbacDecision::kNotApplicable);
        }

        TEST(AbacPolicyEngineTest, AllowRuleWithoutConditions)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "user";
            rule.Resource = "resource";
            rule.Action   = "read";
            rule.Effect   = AbacDecision::kAllow;
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            EXPECT_EQ(
                engine.Evaluate("user", "resource", "read"),
                AbacDecision::kAllow);
        }

        TEST(AbacPolicyEngineTest, DenyRuleWithoutConditions)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "attacker";
            rule.Resource = "*";
            rule.Action   = "*";
            rule.Effect   = AbacDecision::kDeny;
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            EXPECT_EQ(
                engine.Evaluate("attacker", "any_res", "delete"),
                AbacDecision::kDeny);
        }

        TEST(AbacPolicyEngineTest, WildcardMatchesAnySubject)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "public";
            rule.Action   = "read";
            rule.Effect   = AbacDecision::kAllow;
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            EXPECT_EQ(engine.Evaluate("alice", "public", "read"), AbacDecision::kAllow);
            EXPECT_EQ(engine.Evaluate("bob",   "public", "read"), AbacDecision::kAllow);
        }

        TEST(AbacPolicyEngineTest, NonMatchingRuleReturnsNotApplicable)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "alice";
            rule.Resource = "admin";
            rule.Action   = "write";
            rule.Effect   = AbacDecision::kAllow;
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            EXPECT_EQ(
                engine.Evaluate("bob", "admin", "write"),
                AbacDecision::kNotApplicable);
        }

        // -----------------------------------------------------------------------
        // Attribute conditions
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, ConditionEqualsMatch)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "secure";
            rule.Action   = "read";
            rule.Effect   = AbacDecision::kAllow;
            AbacCondition cond;
            cond.AttributeKey = "role";
            cond.Op           = AbacConditionOp::kEquals;
            cond.Value        = "admin";
            rule.Conditions.push_back(cond);
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            AbacAttributes attrsAdmin{{"role", "admin"}};
            AbacAttributes attrsUser{{"role", "user"}};
            AbacAttributes attrsEmpty;

            EXPECT_EQ(engine.Evaluate("x", "secure", "read", attrsAdmin), AbacDecision::kAllow);
            EXPECT_EQ(engine.Evaluate("x", "secure", "read", attrsUser),  AbacDecision::kNotApplicable);
            EXPECT_EQ(engine.Evaluate("x", "secure", "read", attrsEmpty), AbacDecision::kNotApplicable);
        }

        TEST(AbacPolicyEngineTest, ConditionExistsMatch)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "*";
            rule.Action   = "*";
            rule.Effect   = AbacDecision::kAllow;
            AbacCondition cond;
            cond.AttributeKey = "clearance";
            cond.Op           = AbacConditionOp::kExists;
            rule.Conditions.push_back(cond);
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            AbacAttributes withClearance{{"clearance", "top-secret"}};
            AbacAttributes withoutClearance{{"dept", "engineering"}};

            EXPECT_EQ(engine.Evaluate("x", "y", "z", withClearance),    AbacDecision::kAllow);
            EXPECT_EQ(engine.Evaluate("x", "y", "z", withoutClearance), AbacDecision::kNotApplicable);
        }

        TEST(AbacPolicyEngineTest, ConditionNotExistsMatch)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "*";
            rule.Action   = "*";
            rule.Effect   = AbacDecision::kDeny;
            AbacCondition cond;
            cond.AttributeKey = "mfa_verified";
            cond.Op           = AbacConditionOp::kNotExists;
            rule.Conditions.push_back(cond);
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            AbacAttributes withMfa{{"mfa_verified", "true"}};
            AbacAttributes withoutMfa{{"user", "alice"}};

            EXPECT_EQ(engine.Evaluate("x", "y", "z", withMfa),    AbacDecision::kNotApplicable);
            EXPECT_EQ(engine.Evaluate("x", "y", "z", withoutMfa), AbacDecision::kDeny);
        }

        TEST(AbacPolicyEngineTest, ConditionContainsMatch)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "*";
            rule.Action   = "read";
            rule.Effect   = AbacDecision::kAllow;
            AbacCondition cond;
            cond.AttributeKey = "group";
            cond.Op           = AbacConditionOp::kContains;
            cond.Value        = "dev";
            rule.Conditions.push_back(cond);
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            AbacAttributes withDevGroup{{"group", "dev-team"}};
            AbacAttributes withOtherGroup{{"group", "ops-team"}};

            EXPECT_EQ(engine.Evaluate("x", "y", "read", withDevGroup),   AbacDecision::kAllow);
            EXPECT_EQ(engine.Evaluate("x", "y", "read", withOtherGroup), AbacDecision::kNotApplicable);
        }

        // -----------------------------------------------------------------------
        // First-match semantics
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, FirstMatchSemantics)
        {
            AbacPolicyEngine engine;

            // First rule: allow admin
            AbacRule allow;
            allow.Subject  = "admin";
            allow.Resource = "*";
            allow.Action   = "*";
            allow.Effect   = AbacDecision::kAllow;
            ASSERT_TRUE(engine.AddRule(allow).HasValue());

            // Second rule: deny * (catch-all, comes second)
            AbacRule deny;
            deny.Subject  = "*";
            deny.Resource = "*";
            deny.Action   = "*";
            deny.Effect   = AbacDecision::kDeny;
            ASSERT_TRUE(engine.AddRule(deny).HasValue());

            EXPECT_EQ(engine.Evaluate("admin", "x", "y"), AbacDecision::kAllow);
            EXPECT_EQ(engine.Evaluate("user",  "x", "y"), AbacDecision::kDeny);
        }

        // -----------------------------------------------------------------------
        // ClearRules / RuleCount
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, RuleCountAndClear)
        {
            AbacPolicyEngine engine;
            EXPECT_EQ(engine.RuleCount(), 0U);

            AbacRule r1, r2;
            r1.Subject = "a"; r1.Resource = "b"; r1.Action = "c";
            r1.Effect  = AbacDecision::kAllow;
            r2.Subject = "x"; r2.Resource = "y"; r2.Action = "z";
            r2.Effect  = AbacDecision::kDeny;

            ASSERT_TRUE(engine.AddRule(r1).HasValue());
            ASSERT_TRUE(engine.AddRule(r2).HasValue());
            EXPECT_EQ(engine.RuleCount(), 2U);

            engine.ClearRules();
            EXPECT_EQ(engine.RuleCount(), 0U);
        }

        // -----------------------------------------------------------------------
        // AddRule validation
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, AddRuleWithEmptySubjectReturnsError)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "";
            rule.Resource = "res";
            rule.Action   = "act";
            rule.Effect   = AbacDecision::kAllow;
            EXPECT_FALSE(engine.AddRule(rule).HasValue());
        }

        // -----------------------------------------------------------------------
        // Persistence
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, SaveAndLoadRoundTrip)
        {
            const std::string tmpFile{"/tmp/ara_abac_test.txt"};

            AbacPolicyEngine writer;
            AbacRule rule;
            rule.Subject  = "alice";
            rule.Resource = "doc";
            rule.Action   = "read";
            rule.Effect   = AbacDecision::kAllow;
            AbacCondition cond;
            cond.AttributeKey = "dept";
            cond.Op           = AbacConditionOp::kEquals;
            cond.Value        = "engineering";
            rule.Conditions.push_back(cond);
            ASSERT_TRUE(writer.AddRule(rule).HasValue());
            ASSERT_TRUE(writer.SaveToFile(tmpFile).HasValue());

            AbacPolicyEngine reader;
            ASSERT_TRUE(reader.LoadFromFile(tmpFile).HasValue());
            EXPECT_EQ(reader.RuleCount(), 1U);

            AbacAttributes goodAttrs{{"dept", "engineering"}};
            AbacAttributes badAttrs{{"dept", "marketing"}};
            EXPECT_EQ(reader.Evaluate("alice", "doc", "read", goodAttrs), AbacDecision::kAllow);
            EXPECT_EQ(reader.Evaluate("alice", "doc", "read", badAttrs),  AbacDecision::kNotApplicable);

            std::remove(tmpFile.c_str());
        }

        // -----------------------------------------------------------------------
        // Audit callback
        // -----------------------------------------------------------------------
        TEST(AbacPolicyEngineTest, AuditCallbackInvoked)
        {
            AbacPolicyEngine engine;
            AbacRule rule;
            rule.Subject  = "*";
            rule.Resource = "*";
            rule.Action   = "*";
            rule.Effect   = AbacDecision::kAllow;
            ASSERT_TRUE(engine.AddRule(rule).HasValue());

            AbacDecision lastDecision{AbacDecision::kNotApplicable};
            engine.SetAuditCallback(
                [&lastDecision](
                    const std::string &,
                    const std::string &,
                    const std::string &,
                    const AbacAttributes &,
                    AbacDecision d)
                {
                    lastDecision = d;
                });

            engine.Evaluate("x", "y", "z");
            EXPECT_EQ(lastDecision, AbacDecision::kAllow);
        }
    }
}
