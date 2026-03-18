/// @file src/ara/per/redundant_storage.h
/// @brief Dual-partition redundant key-value storage with failover.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PER_REDUNDANT_STORAGE_H
#define PER_REDUNDANT_STORAGE_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        /// @brief Which partition is currently active.
        enum class ActivePartition : uint8_t
        {
            kPrimary = 0,
            kSecondary = 1
        };

        /// @brief Health of a single partition.
        enum class PartitionHealth : uint8_t
        {
            kHealthy = 0,
            kDegraded = 1,
            kCorrupted = 2
        };

        /// @brief Status of the redundant storage.
        struct RedundantStorageStatus
        {
            ActivePartition Active{ActivePartition::kPrimary};
            PartitionHealth PrimaryHealth{PartitionHealth::kHealthy};
            PartitionHealth SecondaryHealth{PartitionHealth::kHealthy};
            uint32_t PrimaryKeyCount{0};
            uint32_t SecondaryKeyCount{0};
            uint64_t SequenceNumber{0};
        };

        /// @brief Redundant key-value storage providing dual-partition
        ///        failover for power-loss resilience.
        class RedundantStorage
        {
        public:
            RedundantStorage() = default;
            ~RedundantStorage() = default;

            /// @brief Store a key-value pair in both partitions.
            core::Result<void> SetValue(
                const std::string &key,
                const std::vector<uint8_t> &value);

            /// @brief Read a value, falling back to secondary if primary fails.
            core::Result<std::vector<uint8_t>> GetValue(
                const std::string &key) const;

            /// @brief Check whether a key exists in either partition.
            bool HasKey(const std::string &key) const;

            /// @brief Remove a key from both partitions.
            core::Result<void> RemoveKey(const std::string &key);

            /// @brief Get all keys in the active partition.
            std::vector<std::string> GetAllKeys() const;

            /// @brief Synchronize secondary partition from primary.
            core::Result<void> SyncPartitions();

            /// @brief Force failover to secondary partition.
            void Failover();

            /// @brief Get current storage status.
            RedundantStorageStatus GetStatus() const;

            /// @brief Simulate primary partition corruption for testing.
            void InjectPrimaryCorruption();

            /// @brief Reset both partitions to healthy empty state.
            void Reset();

        private:
            mutable std::mutex mMutex;
            std::map<std::string, std::vector<uint8_t>> mPrimary;
            std::map<std::string, std::vector<uint8_t>> mSecondary;
            ActivePartition mActive{ActivePartition::kPrimary};
            PartitionHealth mPrimaryHealth{PartitionHealth::kHealthy};
            PartitionHealth mSecondaryHealth{PartitionHealth::kHealthy};
            uint64_t mSeqNo{0};

            const std::map<std::string, std::vector<uint8_t>> &
            ActiveMap() const;

            std::map<std::string, std::vector<uint8_t>> &
            ActiveMap();
        };
    }
}

#endif
