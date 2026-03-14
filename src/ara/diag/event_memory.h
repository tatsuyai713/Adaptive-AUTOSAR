/// @file src/ara/diag/event_memory.h
/// @brief Diagnostic event memory per AUTOSAR AP SWS_Diag.
/// @details Provides EventMemory for storing DTC events with snapshot
///          and extended data records, aging, and displacement.

#ifndef ARA_DIAG_EVENT_MEMORY_H
#define ARA_DIAG_EVENT_MEMORY_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include "../core/result.h"

namespace ara
{
    namespace diag
    {
        /// @brief Snapshot data record associated with a DTC event.
        struct SnapshotRecord
        {
            std::uint16_t recordNumber{0};
            std::vector<std::uint8_t> data;
        };

        /// @brief Extended data record associated with a DTC event.
        struct ExtendedDataRecord
        {
            std::uint16_t recordNumber{0};
            std::vector<std::uint8_t> data;
        };

        /// @brief Entry in event memory for a single DTC.
        struct EventMemoryEntry
        {
            std::uint32_t dtcNumber{0};            ///< DTC number (3-byte UDS DTC)
            std::uint8_t statusByte{0};             ///< UDS DTC status byte
            std::uint16_t agingCounter{0};          ///< Aging counter (decremented when conditions met)
            std::uint16_t occurrenceCounter{0};     ///< How many times DTC was reported
            std::vector<SnapshotRecord> snapshots;  ///< Freeze frame data
            std::vector<ExtendedDataRecord> extData;///< Extended data
        };

        /// @brief Memory type selector.
        enum class EventMemoryType : std::uint8_t
        {
            kPrimary = 0,   ///< Primary fault memory
            kSecondary = 1, ///< Secondary (mirror) fault memory
            kMirror = 2     ///< Mirror memory
        };

        /// @brief Diagnostic event memory (SWS_Diag_00500).
        /// @details Manages DTC event storage with snapshot records,
        ///          extended data, aging, and displacement.
        class EventMemory
        {
        public:
            /// @brief Constructor.
            /// @param type Memory type.
            /// @param maxEntries Maximum number of entries before displacement.
            explicit EventMemory(
                EventMemoryType type = EventMemoryType::kPrimary,
                std::size_t maxEntries = 64);

            ~EventMemory() noexcept = default;

            /// @brief Store or update a DTC entry.
            /// @param dtcNumber DTC number.
            /// @param statusByte UDS status byte.
            /// @returns Void Result on success.
            core::Result<void> StoreEntry(
                std::uint32_t dtcNumber, std::uint8_t statusByte);

            /// @brief Add a snapshot record to a DTC.
            core::Result<void> AddSnapshot(
                std::uint32_t dtcNumber, const SnapshotRecord &record);

            /// @brief Add an extended data record to a DTC.
            core::Result<void> AddExtendedData(
                std::uint32_t dtcNumber, const ExtendedDataRecord &record);

            /// @brief Get a stored entry.
            core::Result<EventMemoryEntry> GetEntry(std::uint32_t dtcNumber) const;

            /// @brief Get all stored entries.
            std::vector<EventMemoryEntry> GetAllEntries() const;

            /// @brief Remove a DTC entry.
            core::Result<void> ClearEntry(std::uint32_t dtcNumber);

            /// @brief Clear all entries.
            void ClearAll();

            /// @brief Apply aging: decrement counters, remove aged-out entries.
            /// @param agingThreshold Entries with agingCounter >= threshold are removed.
            void ApplyAging(std::uint16_t agingThreshold = 40);

            /// @brief Get the number of stored entries.
            std::size_t GetEntryCount() const;

            /// @brief Check if memory is full (overflow).
            bool IsOverflow() const;

            /// @brief Get memory type.
            EventMemoryType GetType() const noexcept { return mType; }

            /// @brief Apply DTC healing: mark DTCs as healed after passing test cycles (SWS_DIAG_00502).
            /// @param dtcNumber DTC to heal.
            /// @param healingThreshold Number of passing operation cycles required.
            /// @returns Void Result on success.
            core::Result<void> HealDtc(
                std::uint32_t dtcNumber, std::uint16_t healingThreshold = 3);

            /// @brief Read extended data records for a specific DTC (SWS_DIAG_00253).
            /// @param dtcNumber DTC number.
            /// @returns Extended data records vector, or error.
            core::Result<std::vector<ExtendedDataRecord>> ReadExtendedDataRecords(
                std::uint32_t dtcNumber) const;

        private:
            EventMemoryType mType;
            std::size_t mMaxEntries;
            std::map<std::uint32_t, EventMemoryEntry> mEntries;
            mutable std::mutex mMutex;
            bool mOverflow{false};

            void displace();
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_EVENT_MEMORY_H
