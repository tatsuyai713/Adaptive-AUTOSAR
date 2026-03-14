/// @file src/ara/iam/abac_policy_engine.cpp
/// @brief Implementation for Attribute-Based Access Control (ABAC) policy engine.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./abac_policy_engine.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace ara
{
    namespace iam
    {
        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        bool AbacPolicyEngine::wildcardMatch(
            const std::string &pattern,
            const std::string &value) noexcept
        {
            if (pattern == "*") return true;
            return pattern == value;
        }

        bool AbacPolicyEngine::evaluateCondition(
            const AbacCondition &condition,
            const AbacAttributes &attributes) noexcept
        {
            const auto it = attributes.find(condition.AttributeKey);
            const bool exists = (it != attributes.end());

            switch (condition.Op)
            {
            case AbacConditionOp::kExists:
                return exists;
            case AbacConditionOp::kNotExists:
                return !exists;
            case AbacConditionOp::kEquals:
                return exists && (it->second == condition.Value);
            case AbacConditionOp::kNotEquals:
                return exists && (it->second != condition.Value);
            case AbacConditionOp::kContains:
                return exists &&
                       (it->second.find(condition.Value) != std::string::npos);
            default:
                return false;
            }
        }

        // -----------------------------------------------------------------------
        // Public API
        // -----------------------------------------------------------------------

        core::Result<void> AbacPolicyEngine::AddRule(const AbacRule &rule)
        {
            if (rule.Subject.empty() || rule.Resource.empty() || rule.Action.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock(mMutex);
            mRules.push_back(rule);
            return core::Result<void>::FromValue();
        }

        void AbacPolicyEngine::ClearRules()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRules.clear();
        }

        std::size_t AbacPolicyEngine::RuleCount() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mRules.size();
        }

        AbacDecision AbacPolicyEngine::Evaluate(
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            const AbacAttributes &attributes) const
        {
            AbacDecision decision{AbacDecision::kNotApplicable};

            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (const auto &rule : mRules)
                {
                    if (!wildcardMatch(rule.Subject, subject)) continue;
                    if (!wildcardMatch(rule.Resource, resource)) continue;
                    if (!wildcardMatch(rule.Action, action)) continue;

                    // Evaluate all conditions (AND semantics)
                    bool conditionsMet{true};
                    for (const auto &cond : rule.Conditions)
                    {
                        if (!evaluateCondition(cond, attributes))
                        {
                            conditionsMet = false;
                            break;
                        }
                    }

                    if (conditionsMet)
                    {
                        decision = rule.Effect;
                        break; // first-match semantics
                    }
                }
            }

            if (mAuditCallback)
            {
                mAuditCallback(subject, resource, action, attributes, decision);
            }

            return decision;
        }

        void AbacPolicyEngine::SetAuditCallback(AuditCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mAuditCallback = std::move(callback);
        }

        // -----------------------------------------------------------------------
        // Persistence: simple text format
        // Format per rule block:
        //   RULE
        //   subject=<value>
        //   resource=<value>
        //   action=<value>
        //   effect=<allow|deny>
        //   condition=<key>:<op>:<value>   (zero or more lines)
        //   END
        // -----------------------------------------------------------------------

        static std::string EncodeEffect(AbacDecision d)
        {
            return (d == AbacDecision::kAllow) ? "allow" : "deny";
        }

        static AbacDecision DecodeEffect(const std::string &s)
        {
            return (s == "allow") ? AbacDecision::kAllow : AbacDecision::kDeny;
        }

        static std::string EncodeOp(AbacConditionOp op)
        {
            switch (op)
            {
            case AbacConditionOp::kEquals:    return "eq";
            case AbacConditionOp::kNotEquals: return "ne";
            case AbacConditionOp::kContains:  return "contains";
            case AbacConditionOp::kExists:    return "exists";
            case AbacConditionOp::kNotExists: return "notexists";
            default:                          return "eq";
            }
        }

        static AbacConditionOp DecodeOp(const std::string &s)
        {
            if (s == "ne")          return AbacConditionOp::kNotEquals;
            if (s == "contains")    return AbacConditionOp::kContains;
            if (s == "exists")      return AbacConditionOp::kExists;
            if (s == "notexists")   return AbacConditionOp::kNotExists;
            return AbacConditionOp::kEquals; // default / "eq"
        }

        core::Result<void> AbacPolicyEngine::SaveToFile(
            const std::string &filePath) const
        {
            std::ofstream ofs(filePath);
            if (!ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto &rule : mRules)
            {
                ofs << "RULE\n";
                ofs << "subject=" << rule.Subject << "\n";
                ofs << "resource=" << rule.Resource << "\n";
                ofs << "action=" << rule.Action << "\n";
                ofs << "effect=" << EncodeEffect(rule.Effect) << "\n";
                for (const auto &cond : rule.Conditions)
                {
                    ofs << "condition=" << cond.AttributeKey
                        << ":" << EncodeOp(cond.Op)
                        << ":" << cond.Value << "\n";
                }
                ofs << "END\n";
            }

            if (!ofs.good())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> AbacPolicyEngine::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream ifs(filePath);
            if (!ifs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::vector<AbacRule> loaded;
            std::string line;
            bool inRule{false};
            AbacRule currentRule;

            while (std::getline(ifs, line))
            {
                if (line.empty()) continue;

                if (line == "RULE")
                {
                    inRule = true;
                    currentRule = AbacRule{};
                    continue;
                }

                if (line == "END")
                {
                    if (inRule)
                    {
                        loaded.push_back(currentRule);
                    }
                    inRule = false;
                    continue;
                }

                if (!inRule) continue;

                const auto eqPos = line.find('=');
                if (eqPos == std::string::npos)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyFileParseError));
                }

                const std::string key{line.substr(0, eqPos)};
                const std::string val{line.substr(eqPos + 1)};

                if (key == "subject")
                {
                    currentRule.Subject = val;
                }
                else if (key == "resource")
                {
                    currentRule.Resource = val;
                }
                else if (key == "action")
                {
                    currentRule.Action = val;
                }
                else if (key == "effect")
                {
                    currentRule.Effect = DecodeEffect(val);
                }
                else if (key == "condition")
                {
                    // format: key:op:value  (value may contain ':')
                    const auto p1 = val.find(':');
                    if (p1 == std::string::npos)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(IamErrc::kPolicyFileParseError));
                    }
                    const auto p2 = val.find(':', p1 + 1);
                    AbacCondition cond;
                    cond.AttributeKey = val.substr(0, p1);
                    if (p2 == std::string::npos)
                    {
                        cond.Op = DecodeOp(val.substr(p1 + 1));
                    }
                    else
                    {
                        cond.Op = DecodeOp(val.substr(p1 + 1, p2 - p1 - 1));
                        cond.Value = val.substr(p2 + 1);
                    }
                    currentRule.Conditions.push_back(cond);
                }
            }

            if (ifs.bad())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &r : loaded)
            {
                mRules.push_back(std::move(r));
            }

            return core::Result<void>::FromValue();
        }

        AbacDecision AbacPolicyEngine::EvaluateWithTime(
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            std::uint8_t currentHour,
            std::uint8_t currentDayOfWeek,
            const AbacAttributes &attributes) const
        {
            // Inject time attributes, then delegate to Evaluate
            AbacAttributes enriched{attributes};
            enriched["time.hour"] = std::to_string(currentHour);
            enriched["time.dayOfWeek"] = std::to_string(currentDayOfWeek);
            return Evaluate(subject, resource, action, enriched);
        }
    }
}
