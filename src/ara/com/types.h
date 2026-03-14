/// @file src/ara/com/types.h
/// @brief Declarations for types.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_TYPES_H
#define ARA_COM_TYPES_H

#include <cstdint>
#include <functional>
#include <vector>

namespace ara
{
    namespace com
    {
        /// @brief Subscription state per AUTOSAR AP SWS_CM_00310
        enum class SubscriptionState : std::uint8_t
        {
            kNotSubscribed = 0U,       ///< Event/field notifier is not subscribed.
            kSubscriptionPending = 1U, ///< Subscribe request sent, awaiting confirmation.
            kSubscribed = 2U           ///< Subscription is active.
        };

        /// @brief Processing mode for incoming method calls per AUTOSAR AP SWS_CM_00198
        enum class MethodCallProcessingMode : std::uint8_t
        {
            kPoll = 0U,              ///< Application polls for pending method calls.
            kEvent = 1U,             ///< Calls are dispatched via event-driven handling.
            kEventSingleThread = 2U  ///< Event-driven handling serialized on one thread.
        };

        /// @brief Event cache update policy per SWS_CM_00701.
        ///        Controls how an event cache manages received samples.
        enum class EventCacheUpdatePolicy : std::uint8_t
        {
            kLastN = 0U,     ///< Keep only the last N samples (overwrite oldest).
            kNewestN = 1U    ///< Keep the newest N, discard from oldest end.
        };

        /// @brief Filter type for event subscription per SWS_CM_00702.
        enum class FilterType : std::uint8_t
        {
            kNone = 0U,        ///< No filtering, all samples are received.
            kThreshold = 1U,   ///< Only samples exceeding a threshold.
            kRange = 2U,       ///< Only samples within a value range.
            kOneEveryN = 3U    ///< Decimation: receive one sample every N.
        };

        /// @brief Event subscription filter configuration per SWS_CM_00702.
        ///        Used to reduce the number of samples delivered to the
        ///        application based on value-based or rate-based criteria.
        struct FilterConfig
        {
            /// @brief Type of filter to apply.
            FilterType Type{FilterType::kNone};

            /// @brief Threshold value (used when Type == kThreshold).
            double ThresholdValue{0.0};

            /// @brief Lower bound of range (used when Type == kRange).
            double RangeLow{0.0};

            /// @brief Upper bound of range (used when Type == kRange).
            double RangeHigh{0.0};

            /// @brief Decimation factor (used when Type == kOneEveryN).
            std::uint32_t DecimationFactor{1U};

            /// @brief Creates a pass-through (no filtering) config.
            static FilterConfig None() noexcept
            {
                return FilterConfig{};
            }

            /// @brief Creates a threshold filter.
            /// @param threshold Minimum value to pass through.
            static FilterConfig Threshold(double threshold) noexcept
            {
                FilterConfig cfg;
                cfg.Type = FilterType::kThreshold;
                cfg.ThresholdValue = threshold;
                return cfg;
            }

            /// @brief Creates a range filter.
            /// @param low Lower bound (inclusive).
            /// @param high Upper bound (inclusive).
            static FilterConfig Range(double low, double high) noexcept
            {
                FilterConfig cfg;
                cfg.Type = FilterType::kRange;
                cfg.RangeLow = low;
                cfg.RangeHigh = high;
                return cfg;
            }

            /// @brief Creates a decimation filter.
            /// @param n Accept one sample every N received.
            static FilterConfig OneEveryN(std::uint32_t n) noexcept
            {
                FilterConfig cfg;
                cfg.Type = FilterType::kOneEveryN;
                cfg.DecimationFactor = (n > 0U) ? n : 1U;
                return cfg;
            }
        };

        /// @brief Handle returned by StartFindService for stopping the search
        class FindServiceHandle
        {
        private:
            std::uint64_t mId;

        public:
            /// @brief Creates a search-handle token.
            /// @param id Opaque identifier assigned by the discovery subsystem.
            explicit FindServiceHandle(std::uint64_t id) noexcept : mId{id} {}

            /// @brief Returns the opaque numeric handle value.
            std::uint64_t GetId() const noexcept { return mId; }

            /// @brief Equality comparison.
            bool operator==(const FindServiceHandle &other) const noexcept
            {
                return mId == other.mId;
            }

            /// @brief Inequality comparison.
            bool operator!=(const FindServiceHandle &other) const noexcept
            {
                return mId != other.mId;
            }
        };

        /// @brief Container type for service handles per AUTOSAR AP SWS_CM_00302
        template <typename HandleType>
        using ServiceHandleContainer = std::vector<HandleType>;

        /// @brief Callback invoked when new event data is available.
        /// @details This is the no-argument receive notification form defined in AP.
        using EventReceiveHandler = std::function<void()>;

        /// @brief Callback invoked when new event data is available (sized form).
        /// @details Per SWS_CM_00318, the argument is the number of new samples.
        using SizedEventReceiveHandler = std::function<void(std::size_t)>;

        /// @brief Callback invoked when subscription state changes.
        using SubscriptionStateChangeHandler =
            std::function<void(SubscriptionState)>;

        /// @brief Callback invoked when service availability changes.
        template <typename HandleType>
        using FindServiceHandler =
            std::function<void(ServiceHandleContainer<HandleType>)>;
    }
}

#endif
