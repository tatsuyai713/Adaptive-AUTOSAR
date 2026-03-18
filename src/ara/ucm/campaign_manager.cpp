/// @file src/ara/ucm/campaign_manager.cpp
/// @brief Implementation for multi-package update campaign orchestration.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./campaign_manager.h"

#include <algorithm>
#include "../crypto/crypto_provider.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

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

        core::Result<std::vector<std::uint8_t>> CampaignManager::SignCampaignManifest(
            const std::string &campaignId,
            const std::string &privateKeyPem)
        {
            if (campaignId.empty() || privateKeyPem.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::string manifestData;
            {
                std::lock_guard<std::mutex> _lock{mMutex};
                auto _it = mCampaigns.find(campaignId);
                if (_it == mCampaigns.end())
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(UcmErrc::kCampaignNotFound));
                }
                // Serialize campaign as canonical string for signing
                for (const auto &_entry : _it->second.Entries)
                {
                    manifestData += _entry.PackageName + "|" +
                                    _entry.TargetCluster + "|" +
                                    _entry.Version + ";";
                }
            }

            // Use SHA-256 + ECDSA/RSA via OpenSSL EVP
            auto digestResult = ara::crypto::ComputeDigest(
                std::vector<std::uint8_t>(manifestData.begin(),
                                          manifestData.end()),
                ara::crypto::DigestAlgorithm::kSha256);

            if (!digestResult.HasValue())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            (void)digestResult; // digest computed above was for illustration

            // Sign the manifest data using ECDSA/RSA with the provided private key.
            BIO *bio = ::BIO_new_mem_buf(
                privateKeyPem.data(),
                static_cast<int>(privateKeyPem.size()));
            if (bio == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }
            EVP_PKEY *pkey = ::PEM_read_bio_PrivateKey(
                bio, nullptr, nullptr, nullptr);
            ::BIO_free(bio);
            if (pkey == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            EVP_MD_CTX *ctx = ::EVP_MD_CTX_new();
            if (ctx == nullptr ||
                ::EVP_DigestSignInit(
                    ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1 ||
                ::EVP_DigestSignUpdate(
                    ctx, manifestData.data(), manifestData.size()) != 1)
            {
                if (ctx) ::EVP_MD_CTX_free(ctx);
                ::EVP_PKEY_free(pkey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::size_t sigLen = 0;
            ::EVP_DigestSignFinal(ctx, nullptr, &sigLen);
            std::vector<std::uint8_t> sig(sigLen);
            if (::EVP_DigestSignFinal(
                    ctx, sig.data(), &sigLen) != 1)
            {
                ::EVP_MD_CTX_free(ctx);
                ::EVP_PKEY_free(pkey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }
            sig.resize(sigLen);
            ::EVP_MD_CTX_free(ctx);
            ::EVP_PKEY_free(pkey);

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(sig));
        }

        core::Result<bool> CampaignManager::VerifyCampaignManifest(
            const std::string &campaignId,
            const std::vector<std::uint8_t> &signature,
            const std::string &publicKeyPem)
        {
            if (campaignId.empty() || signature.empty() ||
                publicKeyPem.empty())
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::string manifestData;
            {
                std::lock_guard<std::mutex> _lock{mMutex};
                auto _it = mCampaigns.find(campaignId);
                if (_it == mCampaigns.end())
                {
                    return core::Result<bool>::FromError(
                        MakeErrorCode(UcmErrc::kCampaignNotFound));
                }
                for (const auto &_entry : _it->second.Entries)
                {
                    manifestData += _entry.PackageName + "|" +
                                    _entry.TargetCluster + "|" +
                                    _entry.Version + ";";
                }
            }

            // Verify the ECDSA/RSA signature with the provided public key.
            BIO *bio = ::BIO_new_mem_buf(
                publicKeyPem.data(),
                static_cast<int>(publicKeyPem.size()));
            if (bio == nullptr)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }
            EVP_PKEY *pkey = ::PEM_read_bio_PUBKEY(
                bio, nullptr, nullptr, nullptr);
            ::BIO_free(bio);
            if (pkey == nullptr)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            EVP_MD_CTX *ctx = ::EVP_MD_CTX_new();
            bool valid = false;
            if (ctx != nullptr &&
                ::EVP_DigestVerifyInit(
                    ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1 &&
                ::EVP_DigestVerifyUpdate(
                    ctx, manifestData.data(), manifestData.size()) == 1)
            {
                valid = (::EVP_DigestVerifyFinal(
                    ctx,
                    signature.data(),
                    signature.size()) == 1);
            }
            if (ctx) ::EVP_MD_CTX_free(ctx);
            ::EVP_PKEY_free(pkey);

            return core::Result<bool>::FromValue(valid);
        }
    }
}
