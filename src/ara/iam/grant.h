/// @file src/ara/iam/grant.h
/// @brief Time-bounded permission grant token.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef GRANT_H
#define GRANT_H

#include <cstdint>
#include <string>

namespace ara
{
    namespace iam
    {
        /// @brief Metadata for a permission grant.
        struct GrantInfo
        {
            std::string GrantId;
            std::string Subject;
            std::string Resource;
            std::string Action;
            std::uint64_t IssuedAtEpochMs;
            std::uint64_t ExpiresAtEpochMs;   ///< 0 = no expiry
            bool Revoked;
        };

        /// @brief A named, time-bounded permission token.
        class Grant
        {
        public:
            /// @brief Construct a grant with all fields.
            Grant(const std::string &grantId,
                  const std::string &subject,
                  const std::string &resource,
                  const std::string &action,
                  std::uint64_t issuedAtEpochMs,
                  std::uint64_t expiresAtEpochMs);

            /// @brief Get grant metadata.
            const GrantInfo &Info() const noexcept;

            /// @brief Check whether the grant is currently valid.
            /// @param nowEpochMs Current time in ms since epoch.
            bool IsValid(std::uint64_t nowEpochMs) const noexcept;

            /// @brief Revoke this grant permanently.
            void Revoke() noexcept;

        private:
            GrantInfo mInfo;
        };
    }
}

#endif
