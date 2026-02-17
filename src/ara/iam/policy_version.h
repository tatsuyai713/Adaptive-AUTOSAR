/// @file src/ara/iam/policy_version.h
/// @brief Policy versioning with snapshot/rollback support.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef POLICY_VERSION_H
#define POLICY_VERSION_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./access_control.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief A serialized snapshot of a policy set.
        struct PolicySnapshot
        {
            std::uint32_t Version;
            std::uint64_t TimestampEpochMs;
            std::string Description;
            std::string SerializedPolicies;
        };

        /// @brief Manages versioned snapshots of IAM policies.
        class PolicyVersionManager
        {
        public:
            /// @brief Create a snapshot of the current AccessControl state.
            /// @param ac AccessControl instance to snapshot.
            /// @param description Human-readable description.
            /// @param timestampEpochMs Snapshot timestamp.
            /// @returns New version number or IAM domain error.
            core::Result<std::uint32_t> CreateSnapshot(
                const AccessControl &ac,
                const std::string &description,
                std::uint64_t timestampEpochMs);

            /// @brief Restore a snapshot into an AccessControl instance.
            /// @param version Version to restore.
            /// @param ac AccessControl instance to restore into.
            core::Result<void> RestoreSnapshot(
                std::uint32_t version,
                AccessControl &ac) const;

            /// @brief Get a specific snapshot by version.
            core::Result<PolicySnapshot> GetSnapshot(
                std::uint32_t version) const;

            /// @brief Get current (latest) version number.
            std::uint32_t GetCurrentVersion() const noexcept;

            /// @brief List all available version numbers.
            std::vector<std::uint32_t> ListVersions() const;

            /// @brief Save all snapshots to a file.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load snapshots from a file.
            core::Result<void> LoadFromFile(const std::string &filePath);

        private:
            mutable std::mutex mMutex;
            std::vector<PolicySnapshot> mSnapshots;
            std::uint32_t mCurrentVersion{0U};
        };
    }
}

#endif
