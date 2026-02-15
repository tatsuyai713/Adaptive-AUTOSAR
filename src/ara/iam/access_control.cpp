/// @file src/ara/iam/access_control.cpp
/// @brief Implementation for access control.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./access_control.h"

#include <array>
#include <functional>
#include <unordered_map>

namespace ara
{
    namespace iam
    {
        namespace
        {
            const char *cWildcard{"*"};

            bool IsArgumentInvalid(const std::string &value) noexcept
            {
                return value.empty();
            }
        }

        std::size_t AccessControl::PolicyKeyHash::operator()(
            const PolicyKey &key) const noexcept
        {
            std::size_t _seed{0U};
            const auto _combine = [&_seed](const std::string &value)
            {
                std::hash<std::string> _hasher;
                const auto _hashValue{_hasher(value)};
                _seed ^= _hashValue + 0x9e3779b9U + (_seed << 6U) + (_seed >> 2U);
            };

            _combine(key.Subject);
            _combine(key.Resource);
            _combine(key.Action);
            return _seed;
        }

        core::Result<void> AccessControl::SetPolicy(
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            PermissionDecision decision)
        {
            if (IsArgumentInvalid(subject) ||
                IsArgumentInvalid(resource) ||
                IsArgumentInvalid(action))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mPolicies[PolicyKey{subject, resource, action}] = decision;
            return core::Result<void>::FromValue();
        }

        core::Result<bool> AccessControl::IsAllowed(
            const std::string &subject,
            const std::string &resource,
            const std::string &action) const
        {
            if (IsArgumentInvalid(subject) ||
                IsArgumentInvalid(resource) ||
                IsArgumentInvalid(action))
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            // Evaluate exact match first, then progressively broader wildcard matches.
            const std::array<PolicyKey, 8U> cLookupOrder{
                PolicyKey{subject, resource, action},
                PolicyKey{subject, resource, cWildcard},
                PolicyKey{subject, cWildcard, action},
                PolicyKey{subject, cWildcard, cWildcard},
                PolicyKey{cWildcard, resource, action},
                PolicyKey{cWildcard, resource, cWildcard},
                PolicyKey{cWildcard, cWildcard, action},
                PolicyKey{cWildcard, cWildcard, cWildcard}};

            for (const auto &_lookupKey : cLookupOrder)
            {
                auto _iterator = mPolicies.find(_lookupKey);
                if (_iterator != mPolicies.end())
                {
                    return core::Result<bool>::FromValue(
                        _iterator->second == PermissionDecision::kAllow);
                }
            }

            // Default deny if no policy is configured for this query.
            return core::Result<bool>::FromValue(false);
        }

        void AccessControl::ClearPolicies()
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mPolicies.clear();
        }
    }
}
