/// @file src/ara/iam/identity_manager.cpp
/// @brief Implementation of PasswordStore and IdentityManager.

#include "./identity_manager.h"
#include "./iam_error_domain.h"
#include "../crypto/crypto_provider.h"
#include <algorithm>

namespace ara
{
    namespace iam
    {
        core::Result<void> PasswordStore::RegisterUser(
            const std::string &userId,
            const std::vector<std::uint8_t> &password)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mCredentials.count(userId) > 0)
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));

            // Generate salt
            auto saltResult = crypto::GenerateRandomBytes(16);
            if (!saltResult.HasValue())
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));

            auto salt = saltResult.Value();

            // Hash = SHA-256(salt || password)
            std::vector<std::uint8_t> salted;
            salted.reserve(salt.size() + password.size());
            salted.insert(salted.end(), salt.begin(), salt.end());
            salted.insert(salted.end(), password.begin(), password.end());

            auto hashResult = crypto::ComputeDigest(salted, crypto::DigestAlgorithm::kSha256);
            if (!hashResult.HasValue())
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));

            CredentialEntry entry;
            entry.userId = userId;
            entry.salt = std::move(salt);
            entry.passwordHash = hashResult.Value();
            mCredentials[userId] = std::move(entry);
            return core::Result<void>{};
        }

        core::Result<bool> PasswordStore::VerifyPassword(
            const std::string &userId,
            const std::vector<std::uint8_t> &password) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCredentials.find(userId);
            if (it == mCredentials.end())
                return core::Result<bool>::FromValue(false);

            const auto &entry = it->second;
            std::vector<std::uint8_t> salted;
            salted.reserve(entry.salt.size() + password.size());
            salted.insert(salted.end(), entry.salt.begin(), entry.salt.end());
            salted.insert(salted.end(), password.begin(), password.end());

            auto hashResult = crypto::ComputeDigest(salted, crypto::DigestAlgorithm::kSha256);
            if (!hashResult.HasValue())
                return core::Result<bool>::FromValue(false);

            return core::Result<bool>::FromValue(hashResult.Value() == entry.passwordHash);
        }

        core::Result<void> PasswordStore::RemoveUser(const std::string &userId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCredentials.find(userId);
            if (it == mCredentials.end())
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kGrantNotFound));
            mCredentials.erase(it);
            return core::Result<void>{};
        }

        bool PasswordStore::HasUser(const std::string &userId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCredentials.count(userId) > 0;
        }

        void IdentityManager::RegisterIdentity(
            std::uint32_t pid, const std::string &identity)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mIdentities[pid] = identity;
        }

        core::Result<std::string> IdentityManager::GetIdentity(std::uint32_t pid) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mIdentities.find(pid);
            if (it == mIdentities.end())
                return core::Result<std::string>::FromError(
                    MakeErrorCode(IamErrc::kGrantNotFound));
            return core::Result<std::string>::FromValue(it->second);
        }

        void IdentityManager::UnregisterIdentity(std::uint32_t pid)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mIdentities.erase(pid);
        }

        core::Result<bool> IdentityManager::AuthenticateProcess(
            std::uint32_t pid,
            const std::vector<std::uint8_t> &credential) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mIdentities.find(pid);
            if (it == mIdentities.end())
            {
                return core::Result<bool>::FromValue(false);
            }
            // Simple credential check: credential must not be empty.
            return core::Result<bool>::FromValue(!credential.empty());
        }
    }
}
