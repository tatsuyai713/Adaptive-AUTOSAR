/// @file src/ara/iam/access_control.cpp
/// @brief Implementation for access control.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./access_control.h"

#include <array>
#include <fstream>
#include <functional>
#include <sstream>
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

            bool _allowed{false};
            for (const auto &_lookupKey : cLookupOrder)
            {
                auto _iterator = mPolicies.find(_lookupKey);
                if (_iterator != mPolicies.end())
                {
                    _allowed = (_iterator->second == PermissionDecision::kAllow);
                    break;
                }
            }

            if (mAuditCallback)
            {
                mAuditCallback(subject, resource, action, _allowed);
            }

            return core::Result<bool>::FromValue(_allowed);
        }

        void AccessControl::ClearPolicies()
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mPolicies.clear();
        }

        core::Result<void> AccessControl::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::ofstream _file{filePath};
            if (!_file.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            for (const auto &_entry : mPolicies)
            {
                const char *_decisionStr =
                    (_entry.second == PermissionDecision::kAllow) ? "allow" : "deny";
                _file << _entry.first.Subject << '|'
                      << _entry.first.Resource << '|'
                      << _entry.first.Action << '|'
                      << _decisionStr << '\n';
            }

            if (_file.fail())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> AccessControl::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream _file{filePath};
            if (!_file.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            std::string _line;
            while (std::getline(_file, _line))
            {
                if (_line.empty())
                {
                    continue;
                }

                std::istringstream _stream{_line};
                std::string _subject, _resource, _action, _decisionStr;

                if (!std::getline(_stream, _subject, '|') ||
                    !std::getline(_stream, _resource, '|') ||
                    !std::getline(_stream, _action, '|') ||
                    !std::getline(_stream, _decisionStr))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyFileParseError));
                }

                if (_subject.empty() || _resource.empty() || _action.empty())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyFileParseError));
                }

                PermissionDecision _decision;
                if (_decisionStr == "allow")
                {
                    _decision = PermissionDecision::kAllow;
                }
                else if (_decisionStr == "deny")
                {
                    _decision = PermissionDecision::kDeny;
                }
                else
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyFileParseError));
                }

                mPolicies[PolicyKey{_subject, _resource, _action}] = _decision;
            }

            return core::Result<void>::FromValue();
        }

        void AccessControl::SetAuditCallback(AuditCallback callback)
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mAuditCallback = std::move(callback);
        }
    }
}
