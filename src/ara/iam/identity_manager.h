/// @file src/ara/iam/identity_manager.h
/// @brief Identity management, password store, and request context per AUTOSAR AP SWS_IAM.
/// @details Provides PasswordStore for credential management, IdentityManager for
///          process authentication, and RequestContext for access-decision context.

#ifndef ARA_IAM_IDENTITY_MANAGER_H
#define ARA_IAM_IDENTITY_MANAGER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"

namespace ara
{
    namespace iam
    {
        /// @brief Hashed credential stored by PasswordStore.
        struct CredentialEntry
        {
            std::string userId;
            std::vector<std::uint8_t> passwordHash; ///< Salted hash
            std::vector<std::uint8_t> salt;
        };

        /// @brief Credential / password store (SWS_IAM_00100).
        /// @details Stores salted password hashes for user authentication.
        class PasswordStore
        {
        public:
            PasswordStore() = default;
            ~PasswordStore() noexcept = default;

            /// @brief Register a new user with a password.
            /// @param userId User identifier.
            /// @param password Raw password bytes.
            /// @returns Void Result on success, error if user exists.
            core::Result<void> RegisterUser(
                const std::string &userId,
                const std::vector<std::uint8_t> &password);

            /// @brief Verify a password against stored hash.
            /// @returns true if match, false otherwise.
            core::Result<bool> VerifyPassword(
                const std::string &userId,
                const std::vector<std::uint8_t> &password) const;

            /// @brief Remove a user.
            core::Result<void> RemoveUser(const std::string &userId);

            /// @brief Check if a user exists.
            bool HasUser(const std::string &userId) const;

        private:
            std::map<std::string, CredentialEntry> mCredentials;
            mutable std::mutex mMutex;
        };

        /// @brief Request context carrying authenticated identity (SWS_IAM_00101).
        struct RequestContext
        {
            std::string authenticatedUserId;   ///< Authenticated identity
            std::string sessionId;             ///< Session identifier
            std::string resourceId;            ///< Requested resource
            std::string action;                ///< Requested action (read/write/execute)
            std::map<std::string, std::string> attributes; ///< Additional attributes
        };

        /// @brief Process identity manager (SWS_IAM_00102).
        /// @details Maps process identities to authenticated subjects.
        class IdentityManager
        {
        public:
            IdentityManager() = default;
            ~IdentityManager() noexcept = default;

            /// @brief Register a process identity.
            /// @param pid Process ID.
            /// @param identity Authenticated identity string.
            void RegisterIdentity(std::uint32_t pid, const std::string &identity);

            /// @brief Look up the identity of a process.
            core::Result<std::string> GetIdentity(std::uint32_t pid) const;

            /// @brief Remove a process identity.
            void UnregisterIdentity(std::uint32_t pid);

            /// @brief Authenticate a process by PID and credential (SWS_IAM_00103).
            /// @param pid      OS process identifier.
            /// @param credential  Authentication credential (e.g., a token).
            /// @returns True if authentication succeeds.
            core::Result<bool> AuthenticateProcess(
                std::uint32_t pid,
                const std::vector<std::uint8_t> &credential) const;

        private:
            std::map<std::uint32_t, std::string> mIdentities;
            mutable std::mutex mMutex;
        };

    } // namespace iam
} // namespace ara

#endif // ARA_IAM_IDENTITY_MANAGER_H
