/// @file src/ara/com/qos.h
/// @brief Quality of Service configuration types per SWS_CM_00920.
/// @details Defines QoS profiles and policies for event communication:
///          reliability (best-effort vs. reliable), history depth,
///          deadline monitoring, and priority configuration.

#ifndef ARA_COM_QOS_H
#define ARA_COM_QOS_H

#include <chrono>
#include <cstdint>

namespace ara
{
    namespace com
    {
        /// @brief Reliability level for event delivery.
        enum class ReliabilityKind : std::uint8_t
        {
            kBestEffort = 0U,   ///< Samples may be lost (UDP-like).
            kReliable = 1U      ///< Samples guaranteed delivered in order (TCP-like).
        };

        /// @brief History policy controlling sample retention.
        enum class HistoryKind : std::uint8_t
        {
            kKeepLast = 0U,     ///< Keep only the last N samples.
            kKeepAll = 1U       ///< Keep all samples until consumed.
        };

        /// @brief Durability policy for late-joining subscribers.
        enum class DurabilityKind : std::uint8_t
        {
            kVolatile = 0U,         ///< No data for late joiners.
            kTransientLocal = 1U,   ///< Publisher retains last value for late joiners.
            kTransient = 2U         ///< Middleware retains data beyond publisher lifetime.
        };

        /// @brief QoS profile for an event or field notifier.
        ///        Configures reliability, buffering, timing, and priority.
        struct EventQosProfile
        {
            /// @brief Reliability level (best-effort or reliable).
            ReliabilityKind Reliability{ReliabilityKind::kBestEffort};

            /// @brief History policy (keep-last or keep-all).
            HistoryKind History{HistoryKind::kKeepLast};

            /// @brief History depth (number of samples to keep when keep-last).
            std::uint32_t HistoryDepth{1U};

            /// @brief Durability for late-joining subscribers.
            DurabilityKind Durability{DurabilityKind::kVolatile};

            /// @brief Maximum expected inter-arrival period (0 = no deadline).
            ///        If a sample is not received within this period, a
            ///        deadline-missed notification is raised.
            std::chrono::milliseconds Deadline{0};

            /// @brief Minimum separation between consecutive samples (0 = no limit).
            ///        Controls minimum time between two successive samples.
            std::chrono::milliseconds MinSeparation{0};

            /// @brief Transport priority (0 = default, higher = higher priority).
            std::uint8_t Priority{0U};

            /// @brief Create a default best-effort profile.
            /// @returns Profile with best-effort reliability, keep-last(1).
            static EventQosProfile BestEffort() noexcept
            {
                return EventQosProfile{};
            }

            /// @brief Create a reliable profile with configurable depth.
            /// @param depth History depth.
            /// @returns Profile with reliable delivery, keep-last(depth).
            static EventQosProfile Reliable(std::uint32_t depth = 10U) noexcept
            {
                EventQosProfile p;
                p.Reliability = ReliabilityKind::kReliable;
                p.HistoryDepth = depth;
                return p;
            }

            /// @brief Create a reliable profile with deadline monitoring.
            /// @param deadline Maximum expected inter-arrival time.
            /// @param depth History depth.
            /// @returns Profile with reliable delivery and deadline.
            static EventQosProfile ReliableWithDeadline(
                std::chrono::milliseconds deadline,
                std::uint32_t depth = 10U) noexcept
            {
                EventQosProfile p;
                p.Reliability = ReliabilityKind::kReliable;
                p.HistoryDepth = depth;
                p.Deadline = deadline;
                return p;
            }
        };

        /// @brief QoS profile for method (RPC) communication.
        struct MethodQosProfile
        {
            /// @brief Maximum response time before timeout.
            std::chrono::milliseconds ResponseTimeout{5000};

            /// @brief Maximum number of concurrent outstanding requests.
            std::uint32_t MaxConcurrentRequests{16U};

            /// @brief Whether to retry failed requests automatically.
            bool AutoRetry{false};

            /// @brief Maximum number of retry attempts.
            std::uint32_t MaxRetries{0U};

            /// @brief Delay between retry attempts.
            std::chrono::milliseconds RetryDelay{100};

            /// @brief Transport priority.
            std::uint8_t Priority{0U};
        };
    }
}

#endif
