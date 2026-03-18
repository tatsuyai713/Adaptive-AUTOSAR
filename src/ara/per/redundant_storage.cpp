/// @file src/ara/per/redundant_storage.cpp
/// @brief Implementation of RedundantStorage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./redundant_storage.h"

namespace ara
{
    namespace per
    {
        core::Result<void> RedundantStorage::SetValue(
            const std::string &key,
            const std::vector<uint8_t> &value)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (key.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kValidationFailed));
            }

            ++mSeqNo;

            // Write to primary if healthy.
            if (mPrimaryHealth != PartitionHealth::kCorrupted)
            {
                mPrimary[key] = value;
            }
            // Write to secondary if healthy.
            if (mSecondaryHealth != PartitionHealth::kCorrupted)
            {
                mSecondary[key] = value;
            }

            // If both are corrupted, return error.
            if (mPrimaryHealth == PartitionHealth::kCorrupted &&
                mSecondaryHealth == PartitionHealth::kCorrupted)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return {};
        }

        core::Result<std::vector<uint8_t>> RedundantStorage::GetValue(
            const std::string &key) const
        {
            std::lock_guard<std::mutex> lock(mMutex);

            // Try active partition first.
            const auto &activeMap = ActiveMap();
            auto it = activeMap.find(key);
            if (it != activeMap.end())
            {
                return it->second;
            }

            // Fallback to the other partition.
            const auto &fallback =
                (mActive == ActivePartition::kPrimary) ? mSecondary : mPrimary;
            it = fallback.find(key);
            if (it != fallback.end())
            {
                return it->second;
            }

            return core::Result<std::vector<uint8_t>>::FromError(
                MakeErrorCode(PerErrc::kKeyNotFound));
        }

        bool RedundantStorage::HasKey(const std::string &key) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mPrimary.count(key) || mSecondary.count(key);
        }

        core::Result<void> RedundantStorage::RemoveKey(
            const std::string &key)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            bool found = false;
            if (mPrimary.erase(key)) found = true;
            if (mSecondary.erase(key)) found = true;
            if (!found)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }
            return {};
        }

        std::vector<std::string> RedundantStorage::GetAllKeys() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            const auto &m = ActiveMap();
            std::vector<std::string> keys;
            keys.reserve(m.size());
            for (const auto &kv : m)
            {
                keys.push_back(kv.first);
            }
            return keys;
        }

        core::Result<void> RedundantStorage::SyncPartitions()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mActive == ActivePartition::kPrimary)
            {
                if (mPrimaryHealth == PartitionHealth::kCorrupted)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PerErrc::kPhysicalStorageFailure));
                }
                mSecondary = mPrimary;
                mSecondaryHealth = PartitionHealth::kHealthy;
            }
            else
            {
                if (mSecondaryHealth == PartitionHealth::kCorrupted)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PerErrc::kPhysicalStorageFailure));
                }
                mPrimary = mSecondary;
                mPrimaryHealth = PartitionHealth::kHealthy;
            }
            return {};
        }

        void RedundantStorage::Failover()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mActive = (mActive == ActivePartition::kPrimary)
                          ? ActivePartition::kSecondary
                          : ActivePartition::kPrimary;
        }

        RedundantStorageStatus RedundantStorage::GetStatus() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            RedundantStorageStatus s;
            s.Active = mActive;
            s.PrimaryHealth = mPrimaryHealth;
            s.SecondaryHealth = mSecondaryHealth;
            s.PrimaryKeyCount = static_cast<uint32_t>(mPrimary.size());
            s.SecondaryKeyCount = static_cast<uint32_t>(mSecondary.size());
            s.SequenceNumber = mSeqNo;
            return s;
        }

        void RedundantStorage::InjectPrimaryCorruption()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPrimaryHealth = PartitionHealth::kCorrupted;
            mPrimary.clear();
            // Automatic failover.
            if (mActive == ActivePartition::kPrimary)
            {
                mActive = ActivePartition::kSecondary;
            }
        }

        void RedundantStorage::Reset()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPrimary.clear();
            mSecondary.clear();
            mActive = ActivePartition::kPrimary;
            mPrimaryHealth = PartitionHealth::kHealthy;
            mSecondaryHealth = PartitionHealth::kHealthy;
            mSeqNo = 0;
        }

        const std::map<std::string, std::vector<uint8_t>> &
        RedundantStorage::ActiveMap() const
        {
            return (mActive == ActivePartition::kPrimary)
                       ? mPrimary
                       : mSecondary;
        }

        std::map<std::string, std::vector<uint8_t>> &
        RedundantStorage::ActiveMap()
        {
            return (mActive == ActivePartition::kPrimary)
                       ? mPrimary
                       : mSecondary;
        }
    }
}
