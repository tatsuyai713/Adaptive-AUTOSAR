/// @file src/ara/ucm/campaign_manager.cpp
/// @brief Implementation for multi-package update campaign orchestration.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./campaign_manager.h"

#include <algorithm>

namespace ara
{
    namespace ucm
    {
        core::Result<std::string> CampaignManager::CreateCampaign(
            const std::string &campaignId,
            const std::vector<SoftwarePackageMetadata> &packages)
        {
            if (campaignId.empty() || packages.empty())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            if (mCampaigns.find(campaignId) != mCampaigns.end())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignAlreadyExists));
            }

            CampaignData _data;
            _data.State = CampaignState::kCreated;
            for (const auto &_pkg : packages)
            {
                CampaignEntry _entry;
                _entry.PackageName = _pkg.PackageName;
                _entry.TargetCluster = _pkg.TargetCluster;
                _entry.Version = _pkg.Version;
                _entry.PackageState = UpdateSessionState::kIdle;
                _data.Entries.push_back(std::move(_entry));
            }

            mCampaigns.emplace(campaignId, std::move(_data));
            return core::Result<std::string>::FromValue(campaignId);
        }

        core::Result<void> CampaignManager::StartCampaign(
            const std::string &campaignId)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mCampaigns.find(campaignId);
            if (_it == mCampaigns.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignNotFound));
            }

            if (_it->second.State != CampaignState::kCreated)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignInvalidState));
            }

            _it->second.State = CampaignState::kInProgress;
            return core::Result<void>::FromValue();
        }

        core::Result<void> CampaignManager::AdvancePackage(
            const std::string &campaignId,
            const std::string &packageName,
            UpdateSessionState newState)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mCampaigns.find(campaignId);
            if (_it == mCampaigns.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignNotFound));
            }

            if (_it->second.State != CampaignState::kInProgress &&
                _it->second.State != CampaignState::kPartiallyComplete)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignInvalidState));
            }

            auto &_entries = _it->second.Entries;
            auto _entryIt = std::find_if(
                _entries.begin(), _entries.end(),
                [&packageName](const CampaignEntry &e)
                { return e.PackageName == packageName; });

            if (_entryIt == _entries.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            _entryIt->PackageState = newState;
            RecalculateCampaignState(_it->second);

            return core::Result<void>::FromValue();
        }

        core::Result<void> CampaignManager::RollbackCampaign(
            const std::string &campaignId)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mCampaigns.find(campaignId);
            if (_it == mCampaigns.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignNotFound));
            }

            _it->second.State = CampaignState::kRolledBack;
            for (auto &_entry : _it->second.Entries)
            {
                _entry.PackageState = UpdateSessionState::kRolledBack;
            }

            return core::Result<void>::FromValue();
        }

        core::Result<CampaignState> CampaignManager::GetCampaignState(
            const std::string &campaignId) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mCampaigns.find(campaignId);
            if (_it == mCampaigns.end())
            {
                return core::Result<CampaignState>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignNotFound));
            }

            return core::Result<CampaignState>::FromValue(_it->second.State);
        }

        core::Result<std::vector<CampaignEntry>> CampaignManager::GetCampaignPackages(
            const std::string &campaignId) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mCampaigns.find(campaignId);
            if (_it == mCampaigns.end())
            {
                return core::Result<std::vector<CampaignEntry>>::FromError(
                    MakeErrorCode(UcmErrc::kCampaignNotFound));
            }

            return core::Result<std::vector<CampaignEntry>>::FromValue(
                _it->second.Entries);
        }

        std::vector<std::string> CampaignManager::ListCampaignIds() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<std::string> _ids;
            _ids.reserve(mCampaigns.size());
            for (const auto &_pair : mCampaigns)
            {
                _ids.push_back(_pair.first);
            }
            return _ids;
        }

        void CampaignManager::RecalculateCampaignState(CampaignData &campaign)
        {
            bool _allActivated{true};
            bool _anyFailed{false};
            bool _anyActivated{false};

            for (const auto &_entry : campaign.Entries)
            {
                if (_entry.PackageState == UpdateSessionState::kActivated)
                {
                    _anyActivated = true;
                }
                else
                {
                    _allActivated = false;
                }

                if (_entry.PackageState == UpdateSessionState::kVerificationFailed ||
                    _entry.PackageState == UpdateSessionState::kCancelled)
                {
                    _anyFailed = true;
                }
            }

            if (_anyFailed)
            {
                campaign.State = CampaignState::kFailed;
            }
            else if (_allActivated)
            {
                campaign.State = CampaignState::kCompleted;
            }
            else if (_anyActivated)
            {
                campaign.State = CampaignState::kPartiallyComplete;
            }
        }
    }
}
