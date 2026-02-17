/// @file src/ara/iam/grant_manager.h
/// @brief Grant management for dynamic permission grant/revocation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef GRANT_MANAGER_H
#define GRANT_MANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./grant.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Manages permission grants: issue, revoke, query, and persist.
        class GrantManager
        {
        public:
            /// @brief Issue a new time-bounded grant.
            /// @param subject Process/app identity.
            /// @param resource Service/resource name.
            /// @param action Operation name.
            /// @param durationMs Grant duration in ms (0 = no expiry).
            /// @param nowEpochMs Current time in ms since epoch.
            /// @returns Unique grant ID or IAM domain error.
            core::Result<std::string> IssueGrant(
                const std::string &subject,
                const std::string &resource,
                const std::string &action,
                std::uint64_t durationMs,
                std::uint64_t nowEpochMs);

            /// @brief Revoke an existing grant by ID.
            core::Result<void> RevokeGrant(const std::string &grantId);

            /// @brief Check whether a grant is currently valid.
            core::Result<bool> IsGrantValid(
                const std::string &grantId,
                std::uint64_t nowEpochMs) const;

            /// @brief Get all grants for a specific subject.
            std::vector<GrantInfo> GetGrantsForSubject(
                const std::string &subject) const;

            /// @brief Remove all expired and revoked grants.
            core::Result<void> PurgeExpired(std::uint64_t nowEpochMs);

            /// @brief Save all grants to a text file.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load grants from a text file.
            core::Result<void> LoadFromFile(const std::string &filePath);

        private:
            mutable std::mutex mMutex;
            std::unordered_map<std::string, Grant> mGrants;
            std::uint64_t mNextGrantSeq{1U};
        };
    }
}

#endif
