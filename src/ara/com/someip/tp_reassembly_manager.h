/// @file src/ara/com/someip/tp_reassembly_manager.h
/// @brief Multi-stream SOME/IP-TP reassembly manager.
/// @details Per PRS_SOMEIP_00724, a SOME/IP endpoint must be able to
///          reassemble multiple concurrent TP streams simultaneously.
///          Each stream is identified by {MessageId, ClientId}.  This
///          manager holds a map of TpReassembler instances and handles
///          timeout cleanup, duplicate segment detection, and stream
///          lifecycle management.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SOMEIP_TP_REASSEMBLY_MANAGER_H
#define ARA_COM_SOMEIP_TP_REASSEMBLY_MANAGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include "./someip_tp.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief Key identifying a unique TP reassembly stream.
            struct TpStreamKey
            {
                std::uint32_t MessageId{0U};
                std::uint16_t ClientId{0U};

                bool operator<(const TpStreamKey &other) const noexcept
                {
                    if (MessageId != other.MessageId)
                    {
                        return MessageId < other.MessageId;
                    }
                    return ClientId < other.ClientId;
                }

                bool operator==(const TpStreamKey &other) const noexcept
                {
                    return MessageId == other.MessageId &&
                           ClientId == other.ClientId;
                }
            };

            /// @brief Callback invoked when a TP stream is fully reassembled.
            using TpReassemblyCallback = std::function<void(
                const TpStreamKey &key,
                std::vector<std::uint8_t> payload)>;

            /// @brief Manages multiple concurrent SOME/IP-TP reassembly streams.
            ///        Thread-safe — all public methods are mutex-protected.
            class TpReassemblyManager
            {
            private:
                std::map<TpStreamKey, TpReassembler> mStreams;
                std::chrono::seconds mTimeout;
                mutable std::mutex mMutex;

            public:
                /// @brief Constructor with configurable timeout.
                /// @param timeout Per-stream reassembly timeout
                explicit TpReassemblyManager(
                    std::chrono::seconds timeout = cDefaultTpTimeout) noexcept;

                /// @brief Feed a TP segment into the appropriate stream.
                /// @param key Stream identifier (MessageId + ClientId)
                /// @param offset Segment byte offset
                /// @param moreSegments True if more segments follow
                /// @param segmentPayload Segment data bytes
                /// @param callback Optional callback when reassembly completes
                /// @returns true if the segment was accepted (not a duplicate)
                bool FeedSegment(
                    const TpStreamKey &key,
                    std::uint32_t offset,
                    bool moreSegments,
                    const std::vector<std::uint8_t> &segmentPayload,
                    TpReassemblyCallback callback = nullptr);

                /// @brief Remove all timed-out streams.
                /// @returns Number of streams removed
                std::size_t CleanupTimedOut() noexcept;

                /// @brief Get current number of active streams.
                std::size_t ActiveStreamCount() const noexcept;

                /// @brief Check if a specific stream has a segment at the given offset.
                /// @details Used for duplicate segment detection.
                bool HasSegmentAt(
                    const TpStreamKey &key,
                    std::uint32_t offset) const noexcept;

                /// @brief Remove a specific stream.
                void RemoveStream(const TpStreamKey &key) noexcept;

                /// @brief Remove all streams.
                void Clear() noexcept;
            };
        }
    }
}

#endif
