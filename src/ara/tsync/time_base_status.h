/// @file src/ara/tsync/time_base_status.h
/// @brief Time base status types per AUTOSAR AP SWS_TimeSync.
/// @details Provides TimeBaseStatusType, UserTimeBaseProvider,
///          and OffsetTimeBase types.

#ifndef ARA_TSYNC_TIME_BASE_STATUS_H
#define ARA_TSYNC_TIME_BASE_STATUS_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include "../core/result.h"
#include "./synchronized_time_base_provider.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Detailed time base status (SWS_TimeSync_00100).
        struct TimeBaseStatusType
        {
            bool syncToGateway{false};           ///< Synchronized to a gateway
            bool syncToSubDomain{false};         ///< Synchronized within sub-domain
            bool globalTimeBasePrecision{false};  ///< Precision flag
            bool timeLeap{false};                ///< Time leap detected
            bool timeoutOccurred{false};          ///< Timeout since last sync
            std::int64_t rateDeviationPpb{0};    ///< Rate deviation in parts-per-billion
        };

        /// @brief A user-defined (application-specific) time base provider (SWS_TimeSync_00101).
        /// @details Allows applications to provide their own time source to the
        ///          time sync framework.
        class UserTimeBaseProvider : public SynchronizedTimeBaseProvider
        {
        public:
            /// @brief Callback signature for obtaining a user-defined time point.
            using TimeSourceFunction = std::function<
                std::chrono::nanoseconds()>;

            /// @brief Constructor.
            /// @param name Provider name.
            /// @param source Function returning the current time as nanoseconds
            ///               since an arbitrary epoch.
            explicit UserTimeBaseProvider(
                const std::string &name,
                TimeSourceFunction source);

            ~UserTimeBaseProvider() noexcept override = default;

            core::Result<void> UpdateTimeBase(TimeSyncClient &client) override;
            bool IsSourceAvailable() const override;
            const char *GetProviderName() const noexcept override;

        private:
            std::string mName;
            TimeSourceFunction mSource;
        };

        /// @brief Offset time base — represents time relative to another base (SWS_TimeSync_00102).
        struct OffsetTimeBase
        {
            std::string referenceTimeBaseName;    ///< Name of the reference time base
            std::chrono::nanoseconds offset{0};   ///< Offset from reference
        };

        /// @brief Leap second information (SWS_TS_00200).
        struct LeapSecondInfo
        {
            std::int32_t currentLeapSeconds{0};       ///< Current UTC-TAI offset
            bool futureLeapSecondPending{false};       ///< A future leap second is known
            bool futureLeapSecondPositive{true};       ///< Positive (insert) vs negative
            std::chrono::system_clock::time_point futureLeapSecondTime; ///< When it takes effect
        };

    } // namespace tsync
} // namespace ara

#endif // ARA_TSYNC_TIME_BASE_STATUS_H
