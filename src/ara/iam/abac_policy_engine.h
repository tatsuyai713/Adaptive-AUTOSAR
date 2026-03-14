/// @file src/ara/iam/abac_policy_engine.h
/// @brief Declarations for Attribute-Based Access Control (ABAC) policy engine.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// ABAC extends the subject/resource/action model of `AccessControl` with
/// arbitrary attribute conditions. A policy rule matches only when:
///   1. The (subject, resource, action) triple matches (wildcards supported), AND
///   2. All attribute conditions in the rule evaluate to true for the supplied
///      request attributes.
///
/// Attribute conditions use one of the operators:
///   - kEquals      : attribute value must equal the condition value
///   - kNotEquals   : attribute value must differ from the condition value
///   - kContains    : attribute value string must contain the condition value
///   - kExists      : attribute key must be present (condition value ignored)
///   - kNotExists   : attribute key must be absent (condition value ignored)

#ifndef ABAC_POLICY_ENGINE_H
#define ABAC_POLICY_ENGINE_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Access decision returned by the ABAC engine.
        enum class AbacDecision : std::uint32_t
        {
            kDeny = 0,    ///< Access denied (no matching allow rule, or explicit deny).
            kAllow = 1,   ///< Access granted by a matching allow rule.
            kNotApplicable = 2 ///< No rule matched; caller may apply a default decision.
        };

        /// @brief Comparison operator for an ABAC attribute condition.
        enum class AbacConditionOp : std::uint32_t
        {
            kEquals = 0,    ///< attr == value
            kNotEquals = 1, ///< attr != value
            kContains = 2,  ///< attr contains value (substring)
            kExists = 3,    ///< attr key is present in request (value ignored)
            kNotExists = 4  ///< attr key is absent from request (value ignored)
        };

        /// @brief A single attribute condition attached to an ABAC rule.
        struct AbacCondition
        {
            std::string AttributeKey;   ///< Name of the attribute to inspect.
            AbacConditionOp Op;         ///< Comparison operator.
            std::string Value;          ///< Expected value (ignored for kExists/kNotExists).
        };

        /// @brief An ABAC policy rule.
        struct AbacRule
        {
            std::string Subject;              ///< Subject pattern (supports '*' wildcard).
            std::string Resource;             ///< Resource pattern (supports '*' wildcard).
            std::string Action;               ///< Action pattern (supports '*' wildcard).
            AbacDecision Effect{AbacDecision::kDeny}; ///< Effect if all conditions match.
            std::vector<AbacCondition> Conditions;    ///< All conditions must be satisfied.
        };

        /// @brief Attribute map type used when evaluating a request.
        using AbacAttributes = std::unordered_map<std::string, std::string>;

        /// @brief ABAC policy engine for Adaptive AUTOSAR IAM.
        ///
        /// Evaluates rules in insertion order (first-match semantics). If no rule
        /// matches the request, `kNotApplicable` is returned. Rules without any
        /// conditions evaluate purely on (subject, resource, action) wildcards.
        class AbacPolicyEngine
        {
        private:
            mutable std::mutex mMutex;
            std::vector<AbacRule> mRules;

            using AuditCallback = std::function<void(
                const std::string &subject,
                const std::string &resource,
                const std::string &action,
                const AbacAttributes &attributes,
                AbacDecision decision)>;

            AuditCallback mAuditCallback;

            static bool wildcardMatch(
                const std::string &pattern,
                const std::string &value) noexcept;

            static bool evaluateCondition(
                const AbacCondition &condition,
                const AbacAttributes &attributes) noexcept;

        public:
            /// @brief Add a new rule at the end of the rule list.
            /// @param rule The rule to append.
            /// @returns Void result, or IAM domain error on invalid rule.
            core::Result<void> AddRule(const AbacRule &rule);

            /// @brief Remove all configured rules.
            void ClearRules();

            /// @brief Return the number of currently configured rules.
            std::size_t RuleCount() const noexcept;

            /// @brief Evaluate an access request against all configured rules.
            /// @param subject   Subject (process / principal) identity.
            /// @param resource  Resource (service / object) name.
            /// @param action    Requested action (method / operation).
            /// @param attributes Additional request attributes for condition evaluation.
            /// @returns AbacDecision (kAllow, kDeny, or kNotApplicable).
            AbacDecision Evaluate(
                const std::string &subject,
                const std::string &resource,
                const std::string &action,
                const AbacAttributes &attributes = {}) const;

            /// @brief Set an audit callback invoked on every Evaluate call.
            /// @param callback Callback receiving (subject, resource, action, attrs, decision).
            void SetAuditCallback(AuditCallback callback);

            /// @brief Persist rules to a text file (human-readable, one rule per block).
            /// @param filePath Path to the output file.
            /// @returns Void result, or IAM domain error on I/O failure.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load rules from a file previously saved by SaveToFile.
            /// @param filePath Path to the input file.
            /// @returns Void result, or IAM domain error on parse or I/O failure.
            core::Result<void> LoadFromFile(const std::string &filePath);
        };
    }
}

#endif // ABAC_POLICY_ENGINE_H
