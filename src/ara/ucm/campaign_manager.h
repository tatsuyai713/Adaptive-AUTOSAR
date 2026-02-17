/// @file src/ara/ucm/campaign_manager.h
/// @brief Multi-package update campaign orchestration.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CAMPAIGN_MANAGER_H
#define CAMPAIGN_MANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./ucm_error_domain.h"
#include "./update_manager.h"

namespace ara
{
    namespace ucm
    {
        /// @brief State of an update campaign.
        enum class CampaignState : std::uint32_t
        {
            kCreated = 0,
            kInProgress = 1,
            kPartiallyComplete = 2,
            kCompleted = 3,
            kFailed = 4,
            kRolledBack = 5
        };

        /// @brief Individual package entry within a campaign.
        struct CampaignEntry
        {
            std::string PackageName;
            std::string TargetCluster;
            std::string Version;
            UpdateSessionState PackageState;
        };

        /// @brief Orchestrates multi-package software update campaigns.
        class CampaignManager
        {
        public:
            /// @brief Create a new campaign with a set of packages.
            core::Result<std::string> CreateCampaign(
                const std::string &campaignId,
                const std::vector<SoftwarePackageMetadata> &packages);

            /// @brief Start executing a campaign.
            core::Result<void> StartCampaign(const std::string &campaignId);

            /// @brief Advance the state of an individual package in a campaign.
            core::Result<void> AdvancePackage(
                const std::string &campaignId,
                const std::string &packageName,
                UpdateSessionState newState);

            /// @brief Rollback an entire campaign.
            core::Result<void> RollbackCampaign(const std::string &campaignId);

            /// @brief Query campaign state.
            core::Result<CampaignState> GetCampaignState(
                const std::string &campaignId) const;

            /// @brief Get all packages within a campaign.
            core::Result<std::vector<CampaignEntry>> GetCampaignPackages(
                const std::string &campaignId) const;

            /// @brief List all campaign IDs.
            std::vector<std::string> ListCampaignIds() const;

        private:
            struct CampaignData
            {
                CampaignState State;
                std::vector<CampaignEntry> Entries;
            };

            mutable std::mutex mMutex;
            std::unordered_map<std::string, CampaignData> mCampaigns;

            void RecalculateCampaignState(CampaignData &campaign);
        };
    }
}

#endif
