/// @file src/ara/iam/policy_version.cpp
/// @brief Implementation for policy versioning.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./policy_version.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ara
{
    namespace iam
    {
        core::Result<std::uint32_t> PolicyVersionManager::CreateSnapshot(
            const AccessControl &ac,
            const std::string &description,
            std::uint64_t timestampEpochMs)
        {
            // Save current policies to a temporary file to capture state
            const std::string _tmpPath{"/tmp/autosar_iam_snapshot_tmp.csv"};
            auto _saveResult = ac.SaveToFile(_tmpPath);
            if (!_saveResult.HasValue())
            {
                return core::Result<std::uint32_t>::FromError(
                    MakeErrorCode(IamErrc::kSnapshotError));
            }

            // Read the serialized content
            std::ifstream _ifs{_tmpPath};
            if (!_ifs.is_open())
            {
                return core::Result<std::uint32_t>::FromError(
                    MakeErrorCode(IamErrc::kSnapshotError));
            }

            std::string _serialized;
            std::string _line;
            while (std::getline(_ifs, _line))
            {
                _serialized += _line + "\n";
            }
            _ifs.close();

            std::lock_guard<std::mutex> _lock{mMutex};

            ++mCurrentVersion;
            PolicySnapshot _snapshot;
            _snapshot.Version = mCurrentVersion;
            _snapshot.TimestampEpochMs = timestampEpochMs;
            _snapshot.Description = description;
            _snapshot.SerializedPolicies = std::move(_serialized);

            mSnapshots.push_back(std::move(_snapshot));

            return core::Result<std::uint32_t>::FromValue(mCurrentVersion);
        }

        core::Result<void> PolicyVersionManager::RestoreSnapshot(
            std::uint32_t version,
            AccessControl &ac) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = std::find_if(
                mSnapshots.begin(), mSnapshots.end(),
                [version](const PolicySnapshot &s)
                { return s.Version == version; });

            if (_it == mSnapshots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kVersionNotFound));
            }

            // Write serialized data to temp file and reload
            const std::string _tmpPath{"/tmp/autosar_iam_restore_tmp.csv"};
            std::ofstream _ofs{_tmpPath};
            if (!_ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kSnapshotError));
            }
            _ofs << _it->SerializedPolicies;
            _ofs.close();

            ac.ClearPolicies();
            return ac.LoadFromFile(_tmpPath);
        }

        core::Result<PolicySnapshot> PolicyVersionManager::GetSnapshot(
            std::uint32_t version) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = std::find_if(
                mSnapshots.begin(), mSnapshots.end(),
                [version](const PolicySnapshot &s)
                { return s.Version == version; });

            if (_it == mSnapshots.end())
            {
                return core::Result<PolicySnapshot>::FromError(
                    MakeErrorCode(IamErrc::kVersionNotFound));
            }

            return core::Result<PolicySnapshot>::FromValue(*_it);
        }

        std::uint32_t PolicyVersionManager::GetCurrentVersion() const noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return mCurrentVersion;
        }

        std::vector<std::uint32_t> PolicyVersionManager::ListVersions() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<std::uint32_t> _versions;
            _versions.reserve(mSnapshots.size());
            for (const auto &_snap : mSnapshots)
            {
                _versions.push_back(_snap.Version);
            }
            return _versions;
        }

        core::Result<void> PolicyVersionManager::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::ofstream _ofs{filePath};
            if (!_ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            for (const auto &_snap : mSnapshots)
            {
                _ofs << "VERSION|" << _snap.Version << "|"
                     << _snap.TimestampEpochMs << "|"
                     << _snap.Description << "\n";

                // Write policy lines indented with POLICY| prefix
                std::istringstream _pss{_snap.SerializedPolicies};
                std::string _pLine;
                while (std::getline(_pss, _pLine))
                {
                    if (!_pLine.empty())
                    {
                        _ofs << "POLICY|" << _pLine << "\n";
                    }
                }
                _ofs << "END_VERSION\n";
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> PolicyVersionManager::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream _ifs{filePath};
            if (!_ifs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mSnapshots.clear();
            mCurrentVersion = 0U;

            PolicySnapshot _current;
            bool _inVersion{false};

            std::string _line;
            while (std::getline(_ifs, _line))
            {
                if (_line.empty())
                {
                    continue;
                }

                if (_line.find("VERSION|") == 0U)
                {
                    _inVersion = true;
                    _current = PolicySnapshot{};
                    _current.SerializedPolicies.clear();

                    std::istringstream _ss{_line.substr(8U)};
                    std::string _tmp;
                    std::getline(_ss, _tmp, '|');
                    _current.Version = static_cast<std::uint32_t>(std::stoul(_tmp));
                    std::getline(_ss, _tmp, '|');
                    _current.TimestampEpochMs = std::stoull(_tmp);
                    std::getline(_ss, _current.Description);
                }
                else if (_line == "END_VERSION" && _inVersion)
                {
                    mSnapshots.push_back(std::move(_current));
                    if (_current.Version > mCurrentVersion)
                    {
                        mCurrentVersion = _current.Version;
                    }
                    _inVersion = false;
                }
                else if (_line.find("POLICY|") == 0U && _inVersion)
                {
                    _current.SerializedPolicies += _line.substr(7U) + "\n";
                }
            }

            // Fix mCurrentVersion from snapshots
            for (const auto &_snap : mSnapshots)
            {
                if (_snap.Version > mCurrentVersion)
                {
                    mCurrentVersion = _snap.Version;
                }
            }

            return core::Result<void>::FromValue();
        }
    }
}
