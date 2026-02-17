/// @file src/ara/nm/nm_coordinator.h
/// @brief Multi-channel NM sleep/wake coordinator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef NM_COORDINATOR_H
#define NM_COORDINATOR_H

#include <cstdint>
#include <functional>
#include <mutex>
#include "../core/result.h"
#include "./network_manager.h"
#include "./nm_error_domain.h"

namespace ara
{
    namespace nm
    {
        /// @brief Coordinated sleep policy.
        enum class CoordinatorPolicy : std::uint8_t
        {
            kAllChannelsSleep = 0,   ///< All channels must be in BusSleep
            kMajoritySleep = 1       ///< Majority of channels in BusSleep
        };

        /// @brief Status of the NM coordinator.
        struct CoordinatorStatus
        {
            bool CoordinatedSleepReady;
            std::uint32_t ActiveChannelCount;
            std::uint32_t SleepReadyChannelCount;
        };

        /// @brief Coordinates bus sleep/wake across multiple NM channels.
        class NmCoordinator
        {
        public:
            /// @brief Construct coordinator bound to a NetworkManager.
            explicit NmCoordinator(
                NetworkManager &nm,
                CoordinatorPolicy policy = CoordinatorPolicy::kAllChannelsSleep);

            /// @brief Request coordinated sleep across all channels.
            core::Result<void> RequestCoordinatedSleep();

            /// @brief Request coordinated wakeup across all channels.
            core::Result<void> RequestCoordinatedWakeup();

            /// @brief Get current coordinator status.
            CoordinatorStatus GetStatus() const;

            /// @brief Tick the coordinator to evaluate sleep readiness.
            void Tick(std::uint64_t nowEpochMs);

            /// @brief Set callback invoked when coordinated sleep is ready.
            void SetSleepReadyCallback(std::function<void()> callback);

        private:
            NetworkManager &mNm;
            CoordinatorPolicy mPolicy;
            mutable std::mutex mMutex;
            bool mSleepRequested{false};
            bool mSleepReadyNotified{false};
            std::function<void()> mSleepReadyCallback;
        };
    }
}

#endif
