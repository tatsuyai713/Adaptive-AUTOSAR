/// @file src/ara/tsync/synchronized_time_base_provider.h
/// @brief Declarations for synchronized time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SYNCHRONIZED_TIME_BASE_PROVIDER_H
#define SYNCHRONIZED_TIME_BASE_PROVIDER_H

#include "../core/result.h"
#include "./time_sync_client.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Abstract interface for external time source providers.
        ///
        /// Concrete implementations (PTP, NTP, etc.) derive from this class
        /// and supply synchronized time references to a TimeSyncClient.
        class SynchronizedTimeBaseProvider
        {
        public:
            virtual ~SynchronizedTimeBaseProvider() noexcept = default;

            /// @brief Query the external time source and update the client.
            /// @param client TimeSyncClient to update with the new reference.
            /// @returns Void Result on success, error if the source is
            ///          unavailable or the query fails.
            virtual core::Result<void> UpdateTimeBase(TimeSyncClient &client) = 0;

            /// @brief Check whether the time source is currently available.
            /// @returns True if the source can be queried.
            virtual bool IsSourceAvailable() const = 0;

            /// @brief Get the human-readable provider name.
            /// @returns Provider name string (e.g. "PTP/gPTP", "NTP").
            virtual const char *GetProviderName() const noexcept = 0;
        };
    }
}

#endif
