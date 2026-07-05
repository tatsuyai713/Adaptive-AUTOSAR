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
            std::vector<StateNotification> notifications;
            NmStateChangeHandler handler;

            {
                std::lock_guard<std::mutex> lock{mMutex};
                mLastTickEpochMs = nowEpochMs;
                notifications.reserve(mChannels.size());

                for (auto &entry : mChannels)
                {
                    auto &channel{entry.second};
                    auto &status{channel.Status};
                    const auto &config{channel.Config};
                    StateNotification notification;

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
                            if (TransitionTo(channel, NmState::kRepeatMessage,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
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
                            const NmState nextState{
                                status.NetworkRequested
                                    ? NmState::kNormalOperation
                                    : NmState::kReadySleep};
                            if (TransitionTo(channel, nextState, nowEpochMs,
                                             &notification))
                            {
                                notifications.push_back(std::move(notification));
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
                            if (TransitionTo(channel, NmState::kReadySleep,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        else if (sinceLast >= config.NmTimeoutMs)
                        {
                            ++status.NmTimeoutCount;
                            // Re-enter RepeatMessage to re-announce.
                            if (TransitionTo(channel, NmState::kRepeatMessage,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        break;
                    }

                    case NmState::kReadySleep:
                    {
                        if (status.NetworkRequested || status.NmMessageReceived)
                        {
                            status.NmMessageReceived = false;
                            if (TransitionTo(channel, NmState::kNormalOperation,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        else if (elapsed >= config.NmTimeoutMs)
                        {
                            if (TransitionTo(channel, NmState::kPrepBusSleep,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        break;
                    }

                    case NmState::kPrepBusSleep:
                    {
                        if (status.NetworkRequested || status.NmMessageReceived)
                        {
                            status.NmMessageReceived = false;
                            if (TransitionTo(channel, NmState::kRepeatMessage,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        else if (elapsed >= config.WaitBusSleepTimeMs)
                        {
                            ++status.BusSleepCount;
                            if (TransitionTo(channel, NmState::kBusSleep,
                                             nowEpochMs, &notification))
                            {
                                notifications.push_back(std::move(notification));
                            }
                        }
                        break;
                    }

                    default:
                        break;
                    }
                }

                handler = mStateChangeHandler;
            }

            if (handler)
            {
                for (const auto &notification : notifications)
                {
                    handler(notification.ChannelName,
                            notification.OldState,
                            notification.NewState);
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

        bool NetworkManager::TransitionTo(
            ChannelRuntime &channel,
            NmState newState,
            std::uint64_t nowEpochMs,
            StateNotification *notification)
        {
            const NmState oldState{channel.Status.State};
            if (oldState == newState)
            {
                return false;
            }

            channel.Status.State = newState;
            channel.Status.Mode = DeriveMode(newState);
            channel.Status.StateEnteredEpochMs = nowEpochMs;

            if (notification != nullptr)
            {
                notification->ChannelName = channel.Config.ChannelName;
                notification->OldState = oldState;
                notification->NewState = newState;
            }
            return true;
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

        core::Result<void> NetworkManager::HandleRemoteSleepIndication(
            const std::string &channelName)
        {
            StateNotification notification;
            NmStateChangeHandler handler;
            bool changed{false};
            {
                std::lock_guard<std::mutex> lock{mMutex};
                auto it{mChannels.find(channelName)};
                if (it == mChannels.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(NmErrc::kInvalidChannel));
                }

                auto &channel{it->second};
                // RSI: remote side requests sleep — transition to PrepBusSleep
                // if we are in ReadySleep or NormalOperation without local request
                if (!channel.Status.NetworkRequested &&
                    (channel.Status.State == NmState::kReadySleep ||
                     channel.Status.State == NmState::kNormalOperation))
                {
                    changed = TransitionTo(channel, NmState::kPrepBusSleep,
                                           CurrentEpochMs(), &notification);
                    handler = mStateChangeHandler;
                }
            }

            if (changed && handler)
            {
                handler(notification.ChannelName,
                        notification.OldState,
                        notification.NewState);
            }
            return core::Result<void>::FromValue();
        }

        core::Result<void> NetworkManager::HandleRepeatMessageRequest(
            const std::string &channelName)
        {
            StateNotification notification;
            NmStateChangeHandler handler;
            bool changed{false};
            {
                std::lock_guard<std::mutex> lock{mMutex};
                auto it{mChannels.find(channelName)};
                if (it == mChannels.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(NmErrc::kInvalidChannel));
                }

                auto &channel{it->second};
                // RMR: force re-enter RepeatMessage state for re-announcement
                if (channel.Status.State != NmState::kBusSleep)
                {
                    changed = TransitionTo(channel, NmState::kRepeatMessage,
                                           CurrentEpochMs(), &notification);
                    handler = mStateChangeHandler;
                }
            }

            if (changed && handler)
            {
                handler(notification.ChannelName,
                        notification.OldState,
                        notification.NewState);
            }
            return core::Result<void>::FromValue();
        }

        bool NetworkManager::IsClusterSleepReady() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            if (mChannels.empty())
            {
                return false;
            }

            for (const auto &entry : mChannels)
            {
                const auto &status = entry.second.Status;
                if (status.State != NmState::kBusSleep &&
                    status.State != NmState::kPrepBusSleep &&
                    status.State != NmState::kReadySleep)
                {
                    return false;
                }
            }
            return true;
        }

        std::uint64_t NetworkManager::CurrentEpochMs() const noexcept
        {
            return mLastTickEpochMs;
        }
    }
}
