/// @file src/ara/ucm/update_history.cpp
/// @brief Implementation for persistent update history log.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./update_history.h"

#include <fstream>
#include <sstream>

namespace ara
{
    namespace ucm
    {
        core::Result<void> UpdateHistory::RecordUpdate(
            const UpdateHistoryEntry &entry)
        {
            if (entry.SessionId.empty() || entry.PackageName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mEntries.push_back(entry);
            return core::Result<void>::FromValue();
        }

        std::vector<UpdateHistoryEntry> UpdateHistory::GetHistory() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return mEntries;
        }

        std::vector<UpdateHistoryEntry> UpdateHistory::GetHistoryForCluster(
            const std::string &cluster) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<UpdateHistoryEntry> _result;
            for (const auto &_entry : mEntries)
            {
                if (_entry.TargetCluster == cluster)
                {
                    _result.push_back(_entry);
                }
            }
            return _result;
        }

        core::Result<void> UpdateHistory::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::ofstream _ofs{filePath};
            if (!_ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kHistoryError));
            }

            for (const auto &_entry : mEntries)
            {
                _ofs << _entry.SessionId << "|"
                     << _entry.PackageName << "|"
                     << _entry.TargetCluster << "|"
                     << _entry.FromVersion << "|"
                     << _entry.ToVersion << "|"
                     << _entry.TimestampEpochMs << "|"
                     << (_entry.Success ? 1 : 0) << "|"
                     << _entry.ErrorDescription << "\n";
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateHistory::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream _ifs{filePath};
            if (!_ifs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kHistoryError));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            std::string _line;
            while (std::getline(_ifs, _line))
            {
                if (_line.empty())
                {
                    continue;
                }

                UpdateHistoryEntry _entry;
                std::istringstream _ss{_line};
                std::string _tmp;

                std::getline(_ss, _entry.SessionId, '|');
                std::getline(_ss, _entry.PackageName, '|');
                std::getline(_ss, _entry.TargetCluster, '|');
                std::getline(_ss, _entry.FromVersion, '|');
                std::getline(_ss, _entry.ToVersion, '|');

                std::getline(_ss, _tmp, '|');
                _entry.TimestampEpochMs = std::stoull(_tmp);

                std::getline(_ss, _tmp, '|');
                _entry.Success = (_tmp == "1");

                std::getline(_ss, _entry.ErrorDescription);

                mEntries.push_back(std::move(_entry));
            }

            return core::Result<void>::FromValue();
        }

        void UpdateHistory::Clear()
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mEntries.clear();
        }
    }
}
