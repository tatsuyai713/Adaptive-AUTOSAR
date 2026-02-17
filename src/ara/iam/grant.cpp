/// @file src/ara/iam/grant.cpp
/// @brief Implementation for permission grant token.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./grant.h"

namespace ara
{
    namespace iam
    {
        Grant::Grant(
            const std::string &grantId,
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            std::uint64_t issuedAtEpochMs,
            std::uint64_t expiresAtEpochMs)
            : mInfo{grantId, subject, resource, action,
                    issuedAtEpochMs, expiresAtEpochMs, false}
        {
        }

        const GrantInfo &Grant::Info() const noexcept
        {
            return mInfo;
        }

        bool Grant::IsValid(std::uint64_t nowEpochMs) const noexcept
        {
            if (mInfo.Revoked)
            {
                return false;
            }

            if (mInfo.ExpiresAtEpochMs != 0U &&
                nowEpochMs >= mInfo.ExpiresAtEpochMs)
            {
                return false;
            }

            return true;
        }

        void Grant::Revoke() noexcept
        {
            mInfo.Revoked = true;
        }
    }
}
