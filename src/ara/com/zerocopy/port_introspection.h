/// @file src/ara/com/zerocopy/port_introspection.h
/// @brief Port introspection types for iceoryx zero-copy transport.
/// @details Provides data types representing publisher and subscriber
///          port states, connection status, and shared memory usage
///          statistics.  These are used for monitoring and diagnostic
///          purposes — they map to iceoryx introspection topics.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_ZEROCOPY_PORT_INTROSPECTION_H
#define ARA_COM_ZEROCOPY_PORT_INTROSPECTION_H

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "./zero_copy.h"

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            /// @brief Publisher/subscriber connection state.
            enum class PortConnectionState : std::uint8_t
            {
                kNotConnected = 0U,
                kConnectRequested = 1U,
                kConnected = 2U,
                kDisconnectRequested = 3U,
                kWaitForOffer = 4U
            };

            /// @brief Publisher port information from introspection.
            struct PublisherPortInfo
            {
                ChannelDescriptor Channel;
                std::string RuntimeName;
                std::uint32_t SubscriberCount{0U};
                std::uint64_t HistoryCapacity{0U};
                bool IsOffered{false};
            };

            /// @brief Subscriber port information from introspection.
            struct SubscriberPortInfo
            {
                ChannelDescriptor Channel;
                std::string RuntimeName;
                std::uint64_t QueueCapacity{0U};
                PortConnectionState ConnectionState{
                    PortConnectionState::kNotConnected};
            };

            /// @brief Shared memory pool usage statistics.
            struct MemoryPoolInfo
            {
                std::uint32_t ChunkSize{0U};
                std::uint32_t TotalChunks{0U};
                std::uint32_t UsedChunks{0U};

                /// @brief Get number of free chunks.
                std::uint32_t FreeChunks() const noexcept
                {
                    return TotalChunks > UsedChunks
                               ? TotalChunks - UsedChunks
                               : 0U;
                }

                /// @brief Get usage percentage [0.0, 1.0].
                double UsageRatio() const noexcept
                {
                    return TotalChunks > 0U
                               ? static_cast<double>(UsedChunks) /
                                     static_cast<double>(TotalChunks)
                               : 0.0;
                }
            };

            /// @brief Shared memory segment usage statistics.
            struct SharedMemoryInfo
            {
                std::string SegmentName;
                std::uint64_t TotalBytes{0U};
                std::uint64_t UsedBytes{0U};
                std::vector<MemoryPoolInfo> Pools;

                /// @brief Get free bytes in the segment.
                std::uint64_t FreeBytes() const noexcept
                {
                    return TotalBytes > UsedBytes
                               ? TotalBytes - UsedBytes
                               : 0U;
                }
            };

            /// @brief Aggregated introspection snapshot.
            struct IntrospectionSnapshot
            {
                std::vector<PublisherPortInfo> Publishers;
                std::vector<SubscriberPortInfo> Subscribers;
                std::vector<SharedMemoryInfo> MemorySegments;
                std::chrono::steady_clock::time_point Timestamp{
                    std::chrono::steady_clock::now()};
            };

            /// @brief Port introspection interface.
            ///        Abstracts iceoryx port/memory introspection topics.
            class PortIntrospection
            {
            public:
                virtual ~PortIntrospection() noexcept = default;

                /// @brief Take a snapshot of all publisher ports.
                virtual std::vector<PublisherPortInfo>
                GetPublisherPorts() const = 0;

                /// @brief Take a snapshot of all subscriber ports.
                virtual std::vector<SubscriberPortInfo>
                GetSubscriberPorts() const = 0;

                /// @brief Take a snapshot of shared memory usage.
                virtual std::vector<SharedMemoryInfo>
                GetMemoryUsage() const = 0;

                /// @brief Take a combined introspection snapshot.
                virtual IntrospectionSnapshot TakeSnapshot() const
                {
                    IntrospectionSnapshot snap;
                    snap.Publishers = GetPublisherPorts();
                    snap.Subscribers = GetSubscriberPorts();
                    snap.MemorySegments = GetMemoryUsage();
                    snap.Timestamp = std::chrono::steady_clock::now();
                    return snap;
                }
            };
        }
    }
}

#endif
