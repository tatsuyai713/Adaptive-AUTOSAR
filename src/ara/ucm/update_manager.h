/// @file src/ara/ucm/update_manager.h
/// @brief Declarations for update manager.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/result.h"
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief Update session state in this ara::ucm subset implementation.
        enum class UpdateSessionState : std::uint32_t
        {
            kIdle = 0,
            kPrepared = 1,
            kPackageStaged = 2,
            kPackageVerified = 3,
            kActivating = 4,
            kActivated = 5,
            kVerificationFailed = 6,
            kRolledBack = 7,
            kCancelled = 8,
            kTransferring = 9
        };

        /// @brief Minimal software package metadata used by UpdateManager.
        struct SoftwarePackageMetadata
        {
            std::string PackageName;
            std::string TargetCluster;
            std::string Version;
        };

        /// @brief Minimal UCM flow manager:
        /// Prepare -> Stage -> Verify -> Activate (or Rollback)
        class UpdateManager
        {
        public:
            /// @brief Callback type for update-session state transitions.
            using StateChangeHandler =
                std::function<void(UpdateSessionState)>;
            /// @brief Callback type for update progress notifications.
            using ProgressHandler =
                std::function<void(std::uint8_t)>;

        private:
            mutable std::mutex mMutex;
            UpdateSessionState mState;
            std::string mSessionId;

            SoftwarePackageMetadata mStagedMetadata;
            std::vector<std::uint8_t> mStagedPayload;
            std::vector<std::uint8_t> mExpectedDigestSha256;

            std::vector<std::uint8_t> mTransferBuffer;
            std::uint64_t mExpectedTransferSize;

            std::unordered_map<std::string, std::string> mClusterActiveVersions;
            std::unordered_map<std::string, std::string> mClusterPreviousVersions;
            std::string mLastActivatedCluster;
            std::uint8_t mProgress;

            StateChangeHandler mStateChangeHandler;
            ProgressHandler mProgressHandler;

            void ResetStagingData();
            static bool IsVersionGreater(
                const std::string &newVersion,
                const std::string &currentVersion);

        public:
            UpdateManager() noexcept;

            /// @brief Prepare a new software update session.
            core::Result<void> PrepareUpdate(const std::string &sessionId);

            /// @brief Stage software package data and expected digest.
            core::Result<void> StageSoftwarePackage(
                const SoftwarePackageMetadata &metadata,
                const std::vector<std::uint8_t> &payload,
                const std::vector<std::uint8_t> &expectedDigestSha256);

            /// @brief Verify staged package payload against expected SHA-256 digest.
            core::Result<void> VerifyStagedSoftwarePackage();

            /// @brief Activate a verified package and update active version.
            core::Result<void> ActivateSoftwarePackage();

            /// @brief Roll back current update session.
            core::Result<void> RollbackSoftwarePackage();

            /// @brief Start incremental transfer of a software package.
            core::Result<void> TransferStart(
                const SoftwarePackageMetadata &metadata,
                std::uint64_t expectedSize,
                const std::vector<std::uint8_t> &expectedDigestSha256);

            /// @brief Append a data chunk during an active transfer.
            core::Result<void> TransferData(
                const std::vector<std::uint8_t> &chunk);

            /// @brief Finalize the transfer, verifying size matches expectation.
            core::Result<void> TransferExit();

            /// @brief Cancel current update session without activation.
            core::Result<void> CancelUpdateSession();

            /// @brief Set callback invoked when update session state changes.
            core::Result<void> SetStateChangeHandler(StateChangeHandler handler);

            /// @brief Remove state change callback.
            void UnsetStateChangeHandler();

            /// @brief Set callback invoked when session progress changes.
            core::Result<void> SetProgressHandler(ProgressHandler handler);

            /// @brief Remove progress callback.
            void UnsetProgressHandler();

            /// @brief Query current update session state.
            UpdateSessionState GetState() const noexcept;

            /// @brief Query active software version.
            std::string GetActiveVersion() const;

            /// @brief Query active software version for a specific cluster.
            core::Result<std::string> GetClusterVersion(
                const std::string &cluster) const;

            /// @brief Query list of known software clusters.
            std::vector<std::string> GetKnownClusters() const;

            /// @brief Query current update session id.
            std::string GetSessionId() const;

            /// @brief Query staged package metadata.
            core::Result<SoftwarePackageMetadata> GetStagedSoftwarePackageMetadata() const;

            /// @brief Query current session progress (0..100).
            std::uint8_t GetProgress() const noexcept;
        };
    }
}

#endif
