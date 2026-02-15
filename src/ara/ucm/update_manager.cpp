/// @file src/ara/ucm/update_manager.cpp
/// @brief Implementation for update manager.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./update_manager.h"

#include <algorithm>
#include <sstream>

#include "../crypto/crypto_provider.h"

namespace ara
{
    namespace ucm
    {
        namespace
        {
            bool IsMetadataValid(const SoftwarePackageMetadata &metadata) noexcept
            {
                return !metadata.PackageName.empty() &&
                       !metadata.TargetCluster.empty() &&
                       !metadata.Version.empty();
            }

            std::vector<std::uint32_t> ParseVersion(const std::string &version)
            {
                std::vector<std::uint32_t> parts;
                std::stringstream stream{version};
                std::string token;
                while (std::getline(stream, token, '.'))
                {
                    if (token.empty())
                    {
                        return {};
                    }

                    std::uint32_t value{0U};
                    for (char c : token)
                    {
                        if (c < '0' || c > '9')
                        {
                            return {};
                        }
                        value = static_cast<std::uint32_t>(
                            value * 10U + static_cast<std::uint32_t>(c - '0'));
                    }

                    parts.push_back(value);
                }

                return parts;
            }

            std::pair<UpdateManager::StateChangeHandler, UpdateManager::ProgressHandler>
            CaptureHandlers(
                const UpdateManager::StateChangeHandler &stateHandler,
                const UpdateManager::ProgressHandler &progressHandler)
            {
                return std::make_pair(stateHandler, progressHandler);
            }

            void Notify(
                const std::pair<UpdateManager::StateChangeHandler, UpdateManager::ProgressHandler> &handlers,
                UpdateSessionState state,
                std::uint8_t progress)
            {
                if (handlers.second)
                {
                    handlers.second(progress);
                }

                if (handlers.first)
                {
                    handlers.first(state);
                }
            }
        }

        UpdateManager::UpdateManager() noexcept
            : mState{UpdateSessionState::kIdle},
              mExpectedTransferSize{0U},
              mProgress{0U}
        {
        }

        bool UpdateManager::IsVersionGreater(
            const std::string &newVersion,
            const std::string &currentVersion)
        {
            if (currentVersion.empty())
            {
                return true;
            }

            const auto newParts = ParseVersion(newVersion);
            const auto currentParts = ParseVersion(currentVersion);
            if (newParts.empty() || currentParts.empty())
            {
                return newVersion > currentVersion;
            }

            const auto count = std::max(newParts.size(), currentParts.size());
            for (std::size_t index = 0U; index < count; ++index)
            {
                const std::uint32_t lhs =
                    index < newParts.size() ? newParts[index] : 0U;
                const std::uint32_t rhs =
                    index < currentParts.size() ? currentParts[index] : 0U;

                if (lhs > rhs)
                {
                    return true;
                }

                if (lhs < rhs)
                {
                    return false;
                }
            }

            return false;
        }

        void UpdateManager::ResetStagingData()
        {
            mStagedMetadata = SoftwarePackageMetadata{};
            mStagedPayload.clear();
            mExpectedDigestSha256.clear();
            mTransferBuffer.clear();
            mExpectedTransferSize = 0U;
        }

        core::Result<void> UpdateManager::PrepareUpdate(
            const std::string &sessionId)
        {
            if (sessionId.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState == UpdateSessionState::kPrepared ||
                    mState == UpdateSessionState::kPackageStaged ||
                    mState == UpdateSessionState::kPackageVerified ||
                    mState == UpdateSessionState::kActivating ||
                    mState == UpdateSessionState::kTransferring)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                mSessionId = sessionId;
                ResetStagingData();
                mState = UpdateSessionState::kPrepared;
                mProgress = 5U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kPrepared, 5U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::StageSoftwarePackage(
            const SoftwarePackageMetadata &metadata,
            const std::vector<std::uint8_t> &payload,
            const std::vector<std::uint8_t> &expectedDigestSha256)
        {
            if (!IsMetadataValid(metadata) ||
                expectedDigestSha256.size() != 32U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kPrepared)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                if (mSessionId.empty())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kNoActiveSession));
                }

                mStagedMetadata = metadata;
                mStagedPayload = payload;
                mExpectedDigestSha256 = expectedDigestSha256;
                mState = UpdateSessionState::kPackageStaged;
                mProgress = 25U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kPackageStaged, 25U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::VerifyStagedSoftwarePackage()
        {
            SoftwarePackageMetadata metadata;
            std::vector<std::uint8_t> payload;
            std::vector<std::uint8_t> expectedDigest;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kPackageStaged)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                metadata = mStagedMetadata;
                payload = mStagedPayload;
                expectedDigest = mExpectedDigestSha256;
            }

            if (!IsMetadataValid(metadata) || expectedDigest.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageNotStaged));
            }

            auto digestResult = crypto::ComputeDigest(
                payload,
                crypto::DigestAlgorithm::kSha256);

            if (!digestResult.HasValue() ||
                digestResult.Value() != expectedDigest)
            {
                std::pair<StateChangeHandler, ProgressHandler> handlers;
                {
                    std::lock_guard<std::mutex> lock{mMutex};
                    mState = UpdateSessionState::kVerificationFailed;
                    handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
                }
                Notify(handlers, UpdateSessionState::kVerificationFailed, GetProgress());

                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kVerificationFailed));
            }

            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                mState = UpdateSessionState::kPackageVerified;
                mProgress = 60U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kPackageVerified, 60U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::ActivateSoftwarePackage()
        {
            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kPackageVerified)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                if (!IsMetadataValid(mStagedMetadata))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kPackageNotStaged));
                }

                const auto clusterIt =
                    mClusterActiveVersions.find(mStagedMetadata.TargetCluster);
                const std::string currentVersion =
                    clusterIt == mClusterActiveVersions.end()
                        ? std::string{}
                        : clusterIt->second;
                if (!IsVersionGreater(mStagedMetadata.Version, currentVersion))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kDowngradeNotAllowed));
                }

                mState = UpdateSessionState::kActivating;
                mProgress = 80U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }
            Notify(handlers, UpdateSessionState::kActivating, 80U);

            {
                std::lock_guard<std::mutex> lock{mMutex};
                const auto cluster = mStagedMetadata.TargetCluster;
                const auto activeIt = mClusterActiveVersions.find(cluster);
                if (activeIt != mClusterActiveVersions.end())
                {
                    mClusterPreviousVersions[cluster] = activeIt->second;
                }

                mClusterActiveVersions[cluster] = mStagedMetadata.Version;
                mLastActivatedCluster = cluster;
                ResetStagingData();
                mState = UpdateSessionState::kActivated;
                mProgress = 100U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kActivated, 100U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::RollbackSoftwarePackage()
        {
            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState == UpdateSessionState::kIdle)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kNoActiveSession));
                }

                if (!mLastActivatedCluster.empty())
                {
                    const auto previousIt = mClusterPreviousVersions.find(mLastActivatedCluster);
                    if (previousIt != mClusterPreviousVersions.end())
                    {
                        mClusterActiveVersions[mLastActivatedCluster] = previousIt->second;
                        mClusterPreviousVersions.erase(previousIt);
                    }
                    else
                    {
                        mClusterActiveVersions.erase(mLastActivatedCluster);
                    }
                }

                ResetStagingData();
                mSessionId.clear();
                mState = UpdateSessionState::kRolledBack;
                mProgress = 0U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kRolledBack, 0U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::TransferStart(
            const SoftwarePackageMetadata &metadata,
            std::uint64_t expectedSize,
            const std::vector<std::uint8_t> &expectedDigestSha256)
        {
            if (!IsMetadataValid(metadata) ||
                expectedDigestSha256.size() != 32U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            if (expectedSize == 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kPrepared)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                if (mSessionId.empty())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kNoActiveSession));
                }

                mStagedMetadata = metadata;
                mExpectedDigestSha256 = expectedDigestSha256;
                mExpectedTransferSize = expectedSize;
                mTransferBuffer.clear();
                mTransferBuffer.reserve(
                    static_cast<std::size_t>(expectedSize));
                mState = UpdateSessionState::kTransferring;
                mProgress = 10U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kTransferring, 10U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::TransferData(
            const std::vector<std::uint8_t> &chunk)
        {
            if (chunk.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::pair<StateChangeHandler, ProgressHandler> handlers;
            std::uint8_t progress{0U};
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kTransferring)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                mTransferBuffer.insert(
                    mTransferBuffer.end(), chunk.begin(), chunk.end());

                const auto transferred =
                    static_cast<double>(mTransferBuffer.size());
                const auto expected =
                    static_cast<double>(mExpectedTransferSize);
                progress = static_cast<std::uint8_t>(
                    10U + static_cast<std::uint8_t>(
                              (transferred / expected) * 15.0));
                if (progress > 24U)
                {
                    progress = 24U;
                }
                mProgress = progress;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kTransferring, progress);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::TransferExit()
        {
            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState != UpdateSessionState::kTransferring)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kInvalidState));
                }

                if (mTransferBuffer.empty())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kTransferError));
                }

                if (mTransferBuffer.size() != static_cast<std::size_t>(mExpectedTransferSize))
                {
                    mTransferBuffer.clear();
                    mExpectedTransferSize = 0U;
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kTransferSizeMismatch));
                }

                mStagedPayload = std::move(mTransferBuffer);
                mTransferBuffer.clear();
                mExpectedTransferSize = 0U;
                mState = UpdateSessionState::kPackageStaged;
                mProgress = 25U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kPackageStaged, 25U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::CancelUpdateSession()
        {
            std::pair<StateChangeHandler, ProgressHandler> handlers;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mState == UpdateSessionState::kIdle)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kNoActiveSession));
                }

                ResetStagingData();
                mSessionId.clear();
                mState = UpdateSessionState::kCancelled;
                mProgress = 0U;
                handlers = CaptureHandlers(mStateChangeHandler, mProgressHandler);
            }

            Notify(handlers, UpdateSessionState::kCancelled, 0U);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateManager::SetStateChangeHandler(
            StateChangeHandler handler)
        {
            if (!handler)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mStateChangeHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        void UpdateManager::UnsetStateChangeHandler()
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mStateChangeHandler = nullptr;
        }

        core::Result<void> UpdateManager::SetProgressHandler(
            ProgressHandler handler)
        {
            if (!handler)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mProgressHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        void UpdateManager::UnsetProgressHandler()
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mProgressHandler = nullptr;
        }

        UpdateSessionState UpdateManager::GetState() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mState;
        }

        std::string UpdateManager::GetActiveVersion() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            if (mLastActivatedCluster.empty())
            {
                return {};
            }

            const auto it = mClusterActiveVersions.find(mLastActivatedCluster);
            if (it == mClusterActiveVersions.end())
            {
                return {};
            }

            return it->second;
        }

        core::Result<std::string> UpdateManager::GetClusterVersion(
            const std::string &cluster) const
        {
            if (cluster.empty())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            const auto it = mClusterActiveVersions.find(cluster);
            if (it == mClusterActiveVersions.end())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(UcmErrc::kClusterNotFound));
            }

            return core::Result<std::string>::FromValue(it->second);
        }

        std::vector<std::string> UpdateManager::GetKnownClusters() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            std::vector<std::string> clusters;
            clusters.reserve(mClusterActiveVersions.size());

            for (const auto &entry : mClusterActiveVersions)
            {
                clusters.push_back(entry.first);
            }

            std::sort(clusters.begin(), clusters.end());
            return clusters;
        }

        std::string UpdateManager::GetSessionId() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mSessionId;
        }

        core::Result<SoftwarePackageMetadata>
        UpdateManager::GetStagedSoftwarePackageMetadata() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            if (!IsMetadataValid(mStagedMetadata))
            {
                return core::Result<SoftwarePackageMetadata>::FromError(
                    MakeErrorCode(UcmErrc::kPackageNotStaged));
            }

            return core::Result<SoftwarePackageMetadata>::FromValue(mStagedMetadata);
        }

        std::uint8_t UpdateManager::GetProgress() const noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mProgress;
        }
    }
}
