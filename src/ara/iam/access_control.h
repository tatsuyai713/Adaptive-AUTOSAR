#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include "../core/result.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Access control decision.
        enum class PermissionDecision : std::uint32_t
        {
            kDeny = 0,
            kAllow = 1
        };

        /// @brief In-memory IAM policy evaluator for ECU process/service access.
        ///
        /// Policy keys:
        /// - subject: process/app identity
        /// - resource: service/resource name
        /// - action: operation name
        ///
        /// Wildcard matching is supported with `*`.
        class AccessControl
        {
        private:
            struct PolicyKey
            {
                std::string Subject;
                std::string Resource;
                std::string Action;

                bool operator==(const PolicyKey &other) const noexcept
                {
                    return Subject == other.Subject &&
                           Resource == other.Resource &&
                           Action == other.Action;
                }
            };

            struct PolicyKeyHash
            {
                std::size_t operator()(const PolicyKey &key) const noexcept;
            };

            mutable std::mutex mMutex;
            std::unordered_map<PolicyKey, PermissionDecision, PolicyKeyHash> mPolicies;

        public:
            /// @brief Register or overwrite one policy entry.
            /// @returns Void result, or IAM domain error.
            core::Result<void> SetPolicy(
                const std::string &subject,
                const std::string &resource,
                const std::string &action,
                PermissionDecision decision);

            /// @brief Evaluate access decision for a query.
            /// @returns `true` for allow, `false` for deny, or IAM error.
            core::Result<bool> IsAllowed(
                const std::string &subject,
                const std::string &resource,
                const std::string &action) const;

            /// @brief Remove all configured policies.
            void ClearPolicies();
        };
    }
}

#endif
