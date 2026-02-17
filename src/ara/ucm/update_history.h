/// @file src/ara/ucm/update_history.h
/// @brief Persistent log of past software updates.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef UPDATE_HISTORY_H
#define UPDATE_HISTORY_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief Record of a single completed software update.
        struct UpdateHistoryEntry
        {
            std::string SessionId;
            std::string PackageName;
            std::string TargetCluster;
            std::string FromVersion;
            std::string ToVersion;
            std::uint64_t TimestampEpochMs;
            bool Success;
            std::string ErrorDescription;
        };

        /// @brief Persistent log of past software updates.
        class UpdateHistory
        {
        public:
            /// @brief Record a completed update.
            core::Result<void> RecordUpdate(const UpdateHistoryEntry &entry);

            /// @brief Get all history entries.
            std::vector<UpdateHistoryEntry> GetHistory() const;

            /// @brief Get history for a specific cluster.
            std::vector<UpdateHistoryEntry> GetHistoryForCluster(
                const std::string &cluster) const;

            /// @brief Save history to a text file.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load history from a text file.
            core::Result<void> LoadFromFile(const std::string &filePath);

            /// @brief Remove all history entries.
            void Clear();

        private:
            mutable std::mutex mMutex;
            std::vector<UpdateHistoryEntry> mEntries;
        };
    }
}

#endif
