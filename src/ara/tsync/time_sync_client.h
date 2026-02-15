#ifndef TIME_SYNC_CLIENT_H
#define TIME_SYNC_CLIENT_H

#include <chrono>
#include <cstdint>
#include <mutex>
#include "../core/result.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Synchronization state for the local time base.
        enum class SynchronizationState : std::uint32_t
        {
            kUnsynchronized = 0,
            kSynchronized = 1
        };

        /// @brief Minimal time synchronization client for ECU applications.
        ///
        /// This class stores a relation between local `steady_clock` and a
        /// provided global/system time reference. Applications can request a
        /// synchronized timestamp for "now" or for a specific local steady time.
        class TimeSyncClient
        {
        private:
            mutable std::mutex mMutex;
            SynchronizationState mState;
            std::chrono::system_clock::duration mOffset;

        public:
            TimeSyncClient() noexcept;

            /// @brief Update synchronization relation using one reference sample.
            /// @param referenceGlobalTime Global/system time of the sample.
            /// @param referenceSteadyTime Local steady time when the sample was taken.
            core::Result<void> UpdateReferenceTime(
                std::chrono::system_clock::time_point referenceGlobalTime,
                std::chrono::steady_clock::time_point referenceSteadyTime =
                    std::chrono::steady_clock::now());

            /// @brief Resolve synchronized global/system time for a local steady time.
            /// @param localSteadyTime Local steady time to convert.
            /// @returns Synchronized system time or `kNotSynchronized` error.
            core::Result<std::chrono::system_clock::time_point> GetCurrentTime(
                std::chrono::steady_clock::time_point localSteadyTime =
                    std::chrono::steady_clock::now()) const;

            /// @brief Get currently applied offset in nanoseconds.
            core::Result<std::chrono::nanoseconds> GetCurrentOffset() const;

            /// @brief Get synchronization state.
            SynchronizationState GetState() const noexcept;

            /// @brief Reset state to unsynchronized.
            void Reset() noexcept;
        };
    }
}

#endif
