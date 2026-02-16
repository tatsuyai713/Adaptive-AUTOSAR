/// @file src/ara/nm/network_manager.cpp
/// @brief Implementation for Network Management channel controller.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./network_manager.h"

#include <algorithm>

namespace ara
{
    namespace nm
    {
        NetworkManager::NetworkManager() noexcept = default;

        core::Result<void> NetworkManager::AddChannel(
            const NmChannelConfig &config)
        {
            if (config.ChannelName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            if (mChannels.find(config.ChannelName) != mChannels.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kAlreadyStarted));
            }

            ChannelRuntime runtime{};
            runtime.Config = config;
            runtime.Status.State = NmState::kBusSleep;
            runtime.Status.Mode = NmMode::kBusSleep;
            mChannels.emplace(config.ChannelName, std::move(runtime));

            return core::Result<void>::FromValue();
        }

        core::Result<void> NetworkManager::RemoveChannel(
            const std::string &channelName)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            auto it{mChannels.find(channelName)};
            if (it == mChannels.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            mChannels.erase(it);
            return core::Result<void>::FromValue();
        }

        core::Result<void> NetworkManager::NetworkRequest(
            const std::string &channelName)
        {
            NmStateChangeHandler handler;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                auto it{mChannels.find(channelName)};
                if (it == mChannels.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(NmErrc::kInvalidChannel));
                }

                auto &channel{it->second};
                channel.Status.NetworkRequested = true;
                handler = mStateChangeHandler;
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> NetworkManager::NetworkRelease(
            const std::string &channelName)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            auto it{mChannels.find(channelName)};
            if (it == mChannels.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            it->second.Status.NetworkRequested = false;
            return core::Result<void>::FromValue();
        }

        core::Result<void> NetworkManager::NmMessageIndication(
            const std::string &channelName)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            auto it{mChannels.find(channelName)};
            if (it == mChannels.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            it->second.Status.NmMessageReceived = true;
            return core::Result<void>::FromValue();
        }

        void NetworkManager::Tick(std::uint64_t nowEpochMs)
        {
            std::lock_guard<std::mutex> lock{mMutex};

            for (auto &entry : mChannels)
            {
                auto &channel{entry.second};
                auto &status{channel.Status};
                const auto &config{channel.Config};

                const std::uint64_t elapsed{
                    (nowEpochMs >= status.StateEnteredEpochMs)
                        ? (nowEpochMs - status.StateEnteredEpochMs)
                        : 0U};

                switch (status.State)
                {
                case NmState::kBusSleep:
                {
                    // Wake up on network request or received NM message.
                    if (status.NetworkRequested || status.NmMessageReceived)
                    {
                        status.NmMessageReceived = false;
                        ++status.WakeupCount;
                        TransitionTo(channel, NmState::kRepeatMessage,
                                     nowEpochMs);
                    }
                    break;
                }

                case NmState::kRepeatMessage:
                {
                    status.NmMessageReceived = false;
                    status.LastNmMessageEpochMs = nowEpochMs;
                    ++status.RepeatMessageCount;

                    if (elapsed >= config.RepeatMessageTimeMs)
                    {
                        if (status.NetworkRequested)
                        {
                            TransitionTo(channel,
                                         NmState::kNormalOperation,
                                         nowEpochMs);
                        }
                        else
                        {
                            TransitionTo(channel,
                                         NmState::kReadySleep,
                                         nowEpochMs);
                        }
                    }
                    break;
                }

                case NmState::kNormalOperation:
                {
                    if (status.NmMessageReceived)
                    {
                        status.NmMessageReceived = false;
                        status.LastNmMessageEpochMs = nowEpochMs;
                    }

                    // NM timeout: no NM messages received within timeout.
                    const std::uint64_t sinceLast{
                        (nowEpochMs >= status.LastNmMessageEpochMs)
                            ? (nowEpochMs - status.LastNmMessageEpochMs)
                            : 0U};

                    if (!status.NetworkRequested)
                    {
                        TransitionTo(channel, NmState::kReadySleep,
                                     nowEpochMs);
                    }
                    else if (sinceLast >= config.NmTimeoutMs)
                    {
                        ++status.NmTimeoutCount;
                        // Re-enter RepeatMessage to re-announce.
                        TransitionTo(channel, NmState::kRepeatMessage,
                                     nowEpochMs);
                    }
                    break;
                }

                case NmState::kReadySleep:
                {
                    if (status.NetworkRequested || status.NmMessageReceived)
                    {
                        status.NmMessageReceived = false;
                        TransitionTo(channel, NmState::kNormalOperation,
                                     nowEpochMs);
                    }
                    else if (elapsed >= config.NmTimeoutMs)
                    {
                        TransitionTo(channel, NmState::kPrepBusSleep,
                                     nowEpochMs);
                    }
                    break;
                }

                case NmState::kPrepBusSleep:
                {
                    if (status.NetworkRequested || status.NmMessageReceived)
                    {
                        status.NmMessageReceived = false;
                        TransitionTo(channel, NmState::kRepeatMessage,
                                     nowEpochMs);
                    }
                    else if (elapsed >= config.WaitBusSleepTimeMs)
                    {
                        ++status.BusSleepCount;
                        TransitionTo(channel, NmState::kBusSleep,
                                     nowEpochMs);
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }

        core::Result<NmChannelStatus> NetworkManager::GetChannelStatus(
            const std::string &channelName) const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            auto it{mChannels.find(channelName)};
            if (it == mChannels.end())
            {
                return core::Result<NmChannelStatus>::FromError(
                    MakeErrorCode(NmErrc::kInvalidChannel));
            }

            return core::Result<NmChannelStatus>::FromValue(
                it->second.Status);
        }

        std::vector<std::string> NetworkManager::GetChannelNames() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            std::vector<std::string> names;
            names.reserve(mChannels.size());

            for (const auto &entry : mChannels)
            {
                names.push_back(entry.first);
            }

            std::sort(names.begin(), names.end());
            return names;
        }

        core::Result<void> NetworkManager::SetStateChangeHandler(
            NmStateChangeHandler handler)
        {
            if (!handler)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mStateChangeHandler = std::move(handler);
            return core::Result<void>::FromValue();
        }

        void NetworkManager::ClearStateChangeHandler() noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mStateChangeHandler = nullptr;
        }

        void NetworkManager::TransitionTo(
            ChannelRuntime &channel,
            NmState newState,
            std::uint64_t nowEpochMs)
        {
            const NmState oldState{channel.Status.State};
            if (oldState == newState)
            {
                return;
            }

            channel.Status.State = newState;
            channel.Status.Mode = DeriveMode(newState);
            channel.Status.StateEnteredEpochMs = nowEpochMs;

            if (mStateChangeHandler)
            {
                mStateChangeHandler(
                    channel.Config.ChannelName, oldState, newState);
            }
        }

        NmMode NetworkManager::DeriveMode(NmState state) const noexcept
        {
            switch (state)
            {
            case NmState::kBusSleep:
                return NmMode::kBusSleep;
            case NmState::kPrepBusSleep:
                return NmMode::kPrepareBusSleep;
            case NmState::kReadySleep:
            case NmState::kNormalOperation:
            case NmState::kRepeatMessage:
                return NmMode::kNetwork;
            default:
                return NmMode::kBusSleep;
            }
        }
    }
}
