/// @file src/ara/iam/grant_manager.cpp
/// @brief Implementation for grant management.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./grant_manager.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ara
{
    namespace iam
    {
        core::Result<std::string> GrantManager::IssueGrant(
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            std::uint64_t durationMs,
            std::uint64_t nowEpochMs)
        {
            if (subject.empty() || resource.empty() || action.empty())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            std::string _grantId = "grant_" + std::to_string(mNextGrantSeq++);
            std::uint64_t _expiresAt =
                (durationMs == 0U) ? 0U : (nowEpochMs + durationMs);

            mGrants.emplace(
                _grantId,
                Grant{_grantId, subject, resource, action,
                      nowEpochMs, _expiresAt});

            return core::Result<std::string>::FromValue(_grantId);
        }

        core::Result<void> GrantManager::RevokeGrant(
            const std::string &grantId)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mGrants.find(grantId);
            if (_it == mGrants.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kGrantNotFound));
            }

            _it->second.Revoke();
            return core::Result<void>::FromValue();
        }

        core::Result<bool> GrantManager::IsGrantValid(
            const std::string &grantId,
            std::uint64_t nowEpochMs) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mGrants.find(grantId);
            if (_it == mGrants.end())
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(IamErrc::kGrantNotFound));
            }

            return core::Result<bool>::FromValue(
                _it->second.IsValid(nowEpochMs));
        }

        std::vector<GrantInfo> GrantManager::GetGrantsForSubject(
            const std::string &subject) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<GrantInfo> _result;
            for (const auto &_pair : mGrants)
            {
                if (_pair.second.Info().Subject == subject)
                {
                    _result.push_back(_pair.second.Info());
                }
            }
            return _result;
        }

        core::Result<void> GrantManager::PurgeExpired(
            std::uint64_t nowEpochMs)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mGrants.begin();
            while (_it != mGrants.end())
            {
                if (!_it->second.IsValid(nowEpochMs))
                {
                    _it = mGrants.erase(_it);
                }
                else
                {
                    ++_it;
                }
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> GrantManager::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::ofstream _ofs{filePath};
            if (!_ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            for (const auto &_pair : mGrants)
            {
                const auto &_info = _pair.second.Info();
                _ofs << _info.GrantId << "|"
                     << _info.Subject << "|"
                     << _info.Resource << "|"
                     << _info.Action << "|"
                     << _info.IssuedAtEpochMs << "|"
                     << _info.ExpiresAtEpochMs << "|"
                     << (_info.Revoked ? 1 : 0) << "\n";
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> GrantManager::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream _ifs{filePath};
            if (!_ifs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            std::string _line;
            while (std::getline(_ifs, _line))
            {
                if (_line.empty())
                {
                    continue;
                }

                std::istringstream _ss{_line};
                std::string _grantId, _subject, _resource, _action;
                std::uint64_t _issuedAt{0U}, _expiresAt{0U};
                int _revoked{0};

                std::getline(_ss, _grantId, '|');
                std::getline(_ss, _subject, '|');
                std::getline(_ss, _resource, '|');
                std::getline(_ss, _action, '|');

                std::string _tmp;
                std::getline(_ss, _tmp, '|');
                _issuedAt = std::stoull(_tmp);
                std::getline(_ss, _tmp, '|');
                _expiresAt = std::stoull(_tmp);
                std::getline(_ss, _tmp, '|');
                _revoked = std::stoi(_tmp);

                Grant _grant{_grantId, _subject, _resource, _action,
                             _issuedAt, _expiresAt};
                if (_revoked != 0)
                {
                    _grant.Revoke();
                }

                mGrants.erase(_grantId);
                mGrants.emplace(_grantId, std::move(_grant));

                // Track max sequence number
                if (_grantId.find("grant_") == 0U)
                {
                    try
                    {
                        std::uint64_t _seq = std::stoull(
                            _grantId.substr(6U));
                        if (_seq >= mNextGrantSeq)
                        {
                            mNextGrantSeq = _seq + 1U;
                        }
                    }
                    catch (...)
                    {
                    }
                }
            }

            return core::Result<void>::FromValue();
        }
    }
}
