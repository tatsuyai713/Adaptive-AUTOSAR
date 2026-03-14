/// @file src/ara/diag/event_memory.cpp
/// @brief Implementation of diagnostic event memory.

#include "./event_memory.h"
#include "./diag_error_domain.h"
#include <algorithm>

namespace ara
{
    namespace diag
    {
        EventMemory::EventMemory(EventMemoryType type, std::size_t maxEntries)
            : mType{type}, mMaxEntries{maxEntries}
        {
        }

        core::Result<void> EventMemory::StoreEntry(
            std::uint32_t dtcNumber, std::uint8_t statusByte)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntries.find(dtcNumber);
            if (it != mEntries.end())
            {
                it->second.statusByte = statusByte;
                it->second.occurrenceCounter++;
                it->second.agingCounter = 0; // Reset aging
                return core::Result<void>{};
            }

            if (mEntries.size() >= mMaxEntries)
            {
                displace();
                mOverflow = true;
            }

            EventMemoryEntry entry;
            entry.dtcNumber = dtcNumber;
            entry.statusByte = statusByte;
            entry.occurrenceCounter = 1;
            entry.agingCounter = 0;
            mEntries[dtcNumber] = std::move(entry);
            return core::Result<void>{};
        }

        core::Result<void> EventMemory::AddSnapshot(
            std::uint32_t dtcNumber, const SnapshotRecord &record)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntries.find(dtcNumber);
            if (it == mEntries.end())
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kNoSuchDTC));
            it->second.snapshots.push_back(record);
            return core::Result<void>{};
        }

        core::Result<void> EventMemory::AddExtendedData(
            std::uint32_t dtcNumber, const ExtendedDataRecord &record)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntries.find(dtcNumber);
            if (it == mEntries.end())
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kNoSuchDTC));
            it->second.extData.push_back(record);
            return core::Result<void>{};
        }

        core::Result<EventMemoryEntry> EventMemory::GetEntry(std::uint32_t dtcNumber) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntries.find(dtcNumber);
            if (it == mEntries.end())
                return core::Result<EventMemoryEntry>::FromError(
                    MakeErrorCode(DiagErrc::kNoSuchDTC));
            return core::Result<EventMemoryEntry>::FromValue(it->second);
        }

        std::vector<EventMemoryEntry> EventMemory::GetAllEntries() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<EventMemoryEntry> result;
            result.reserve(mEntries.size());
            for (const auto &pair : mEntries)
                result.push_back(pair.second);
            return result;
        }

        core::Result<void> EventMemory::ClearEntry(std::uint32_t dtcNumber)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntries.find(dtcNumber);
            if (it == mEntries.end())
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kNoSuchDTC));
            mEntries.erase(it);
            return core::Result<void>{};
        }

        void EventMemory::ClearAll()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntries.clear();
            mOverflow = false;
        }

        void EventMemory::ApplyAging(std::uint16_t agingThreshold)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto it = mEntries.begin(); it != mEntries.end();)
            {
                it->second.agingCounter++;
                if (it->second.agingCounter >= agingThreshold)
                    it = mEntries.erase(it);
                else
                    ++it;
            }
        }

        std::size_t EventMemory::GetEntryCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEntries.size();
        }

        bool EventMemory::IsOverflow() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mOverflow;
        }

        void EventMemory::displace()
        {
            // Displacement policy: remove oldest (lowest occurrence) entry
            if (mEntries.empty()) return;
            auto oldest = mEntries.begin();
            for (auto it = mEntries.begin(); it != mEntries.end(); ++it)
            {
                if (it->second.occurrenceCounter < oldest->second.occurrenceCounter)
                    oldest = it;
            }
            mEntries.erase(oldest);
        }
    }
}
