/// @file src/ara/nm/network_manager.h
/// @brief Network Management (NM) channel controller.
/// @details Implements AUTOSAR-style NM coordinated bus sleep/wake,
///          partial networking, and NM state machine per channel.

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./nm_error_domain.h"

namespace ara
{
    namespace nm
    {
        /// NM channel states (simplified AUTOSAR NM state machine).
        enum class NmState : std::uint8_t
        {
            kBusSleep = 0,
            kPrepBusSleep = 1,
            kReadySleep = 2,
            kNormalOperation = 3,
            kRepeatMessage = 4
        };

        /// NM network mode reported externally.
        enum class NmMode : std::uint8_t
        {
            kBusSleep = 0,
            kPrepareBusSleep = 1,
            kNetwork = 2
        };

        /// Notification callback for NM state changes.
        using NmStateChangeHandler =
            std::function<void(const std::string &channelName,
                               NmState oldState,
                               NmState newState)>;

        /// Per-channel NM configuration.
        struct NmChannelConfig
        {
            std::string ChannelName;
            std::uint32_t NmTimeoutMs{5000U};
            std::uint32_t RepeatMessageTimeMs{1500U};
            std::uint32_t WaitBusSleepTimeMs{2000U};
            bool PartialNetworkEnabled{false};
        };

        /// Runtime state of an NM channel.
        struct NmChannelStatus
        {
            NmState State{NmState::kBusSleep};
            NmMode Mode{NmMode::kBusSleep};
            bool NetworkRequested{false};
            bool NmMessageReceived{false};
            std::uint64_t LastNmMessageEpochMs{0U};
            std::uint64_t StateEnteredEpochMs{0U};
            std::uint32_t RepeatMessageCount{0U};
            std::uint32_t NmTimeoutCount{0U};
            std::uint32_t BusSleepCount{0U};
            std::uint32_t WakeupCount{0U};
        };

        /// Multi-channel NM controller.
        class NetworkManager
        {
        public:
            NetworkManager() noexcept;

            core::Result<void> AddChannel(const NmChannelConfig &config);
            core::Result<void> RemoveChannel(const std::string &channelName);

            core::Result<void> NetworkRequest(const std::string &channelName);
            core::Result<void> NetworkRelease(const std::string &channelName);

            /// Called when an NM PDU is received on the bus.
            core::Result<void> NmMessageIndication(
                const std::string &channelName);

            /// Tick the NM state machine for all channels.
            /// @param nowEpochMs  current time in milliseconds since epoch.
            void Tick(std::uint64_t nowEpochMs);

            core::Result<NmChannelStatus> GetChannelStatus(
                const std::string &channelName) const;

            std::vector<std::string> GetChannelNames() const;

            core::Result<void> SetStateChangeHandler(
                NmStateChangeHandler handler);
            void ClearStateChangeHandler() noexcept;

        private:
            struct ChannelRuntime
            {
                NmChannelConfig Config;
                NmChannelStatus Status;
            };

            void TransitionTo(
                ChannelRuntime &channel,
                NmState newState,
                std::uint64_t nowEpochMs);

            NmMode DeriveMode(NmState state) const noexcept;

            mutable std::mutex mMutex;
            std::unordered_map<std::string, ChannelRuntime> mChannels;
            NmStateChangeHandler mStateChangeHandler;
        };
    }
}

#endif
