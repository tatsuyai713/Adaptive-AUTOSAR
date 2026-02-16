/// @file src/main_network_manager.cpp
/// @brief Resident daemon that runs AUTOSAR-style Network Management (NM)
///        state machines for configured channels and coordinates bus
///        sleep/wake behaviour.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/nm/network_manager.h"

namespace
{
    std::atomic_bool gRunning{true};

    void RequestStop(int) noexcept
    {
        gRunning = false;
    }

    std::string GetEnvOrDefault(const char *key, std::string fallback)
    {
        const char *value{std::getenv(key)};
        if (value != nullptr)
        {
            return value;
        }

        return fallback;
    }

    std::uint32_t GetEnvU32(const char *key, std::uint32_t fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(value)};
            if (parsed == 0U || parsed > 600000U)
            {
                return fallback;
            }
            return static_cast<std::uint32_t>(parsed);
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool GetEnvBool(const char *key, bool fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        const std::string text{value};
        if (text == "1" || text == "true" || text == "TRUE" || text == "on")
        {
            return true;
        }
        if (text == "0" || text == "false" || text == "FALSE" || text == "off")
        {
            return false;
        }

        return fallback;
    }

    std::uint64_t NowEpochMs()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    const char *NmStateToString(ara::nm::NmState state)
    {
        switch (state)
        {
        case ara::nm::NmState::kBusSleep:
            return "BusSleep";
        case ara::nm::NmState::kPrepBusSleep:
            return "PrepBusSleep";
        case ara::nm::NmState::kReadySleep:
            return "ReadySleep";
        case ara::nm::NmState::kNormalOperation:
            return "NormalOperation";
        case ara::nm::NmState::kRepeatMessage:
            return "RepeatMessage";
        default:
            return "Unknown";
        }
    }

    const char *NmModeToString(ara::nm::NmMode mode)
    {
        switch (mode)
        {
        case ara::nm::NmMode::kBusSleep:
            return "BusSleep";
        case ara::nm::NmMode::kPrepareBusSleep:
            return "PrepareBusSleep";
        case ara::nm::NmMode::kNetwork:
            return "Network";
        default:
            return "Unknown";
        }
    }

    /// Parse comma-separated channel names from environment variable.
    std::vector<std::string> ParseChannelList(const std::string &text)
    {
        std::vector<std::string> channels;
        std::istringstream stream{text};
        std::string token;
        while (std::getline(stream, token, ','))
        {
            // Trim.
            const auto begin{
                token.find_first_not_of(" \t")};
            if (begin == std::string::npos)
            {
                continue;
            }
            const auto end{token.find_last_not_of(" \t")};
            channels.push_back(token.substr(begin, end - begin + 1U));
        }
        return channels;
    }

    /// Check if an NM wakeup trigger file exists (e.g. written by CAN
    /// interface manager or an external source). Reading and removing
    /// the file counts as receiving an NM PDU indication.
    bool CheckAndConsumeWakeupTrigger(const std::string &triggerDir,
                                      const std::string &channelName)
    {
        const std::string path{triggerDir + "/" + channelName + ".wakeup"};
        struct stat st {};
        if (::stat(path.c_str(), &st) != 0)
        {
            return false;
        }

        // Consume the trigger.
        ::remove(path.c_str());
        return true;
    }

    void WriteStatus(
        const std::string &statusFile,
        ara::nm::NetworkManager &manager)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        const auto channels{manager.GetChannelNames()};
        stream << "channel_count=" << channels.size() << "\n";

        for (std::size_t index = 0U; index < channels.size(); ++index)
        {
            const auto result{manager.GetChannelStatus(channels[index])};
            if (!result.HasValue())
            {
                continue;
            }

            const auto &status{result.Value()};
            const std::string prefix{
                "channel[" + std::to_string(index) + "]."};
            stream << prefix << "name=" << channels[index] << "\n";
            stream << prefix << "state="
                   << NmStateToString(status.State) << "\n";
            stream << prefix << "mode="
                   << NmModeToString(status.Mode) << "\n";
            stream << prefix << "network_requested="
                   << (status.NetworkRequested ? "true" : "false") << "\n";
            stream << prefix << "repeat_message_count="
                   << status.RepeatMessageCount << "\n";
            stream << prefix << "nm_timeout_count="
                   << status.NmTimeoutCount << "\n";
            stream << prefix << "bus_sleep_count="
                   << status.BusSleepCount << "\n";
            stream << prefix << "wakeup_count="
                   << status.WakeupCount << "\n";
        }

        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string channelListText{
        GetEnvOrDefault("AUTOSAR_NM_CHANNELS", "can0")};
    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_NM_PERIOD_MS", 100U)};
    const std::uint32_t nmTimeoutMs{
        GetEnvU32("AUTOSAR_NM_TIMEOUT_MS", 5000U)};
    const std::uint32_t repeatMessageTimeMs{
        GetEnvU32("AUTOSAR_NM_REPEAT_MSG_TIME_MS", 1500U)};
    const std::uint32_t waitBusSleepTimeMs{
        GetEnvU32("AUTOSAR_NM_WAIT_BUS_SLEEP_MS", 2000U)};
    const bool autoRequest{
        GetEnvBool("AUTOSAR_NM_AUTO_REQUEST", true)};
    const bool partialNetworking{
        GetEnvBool("AUTOSAR_NM_PARTIAL_NETWORKING", false)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_NM_STATUS_FILE",
            "/run/autosar/network_manager.status")};
    const std::string triggerDir{
        GetEnvOrDefault(
            "AUTOSAR_NM_TRIGGER_DIR",
            "/run/autosar/nm_triggers")};

    EnsureRunDirectory();
    ::mkdir(triggerDir.c_str(), 0755);

    ara::nm::NetworkManager manager;
    const auto channels{ParseChannelList(channelListText)};

    for (const auto &channelName : channels)
    {
        ara::nm::NmChannelConfig config;
        config.ChannelName = channelName;
        config.NmTimeoutMs = nmTimeoutMs;
        config.RepeatMessageTimeMs = repeatMessageTimeMs;
        config.WaitBusSleepTimeMs = waitBusSleepTimeMs;
        config.PartialNetworkEnabled = partialNetworking;

        (void)manager.AddChannel(config);

        if (autoRequest)
        {
            (void)manager.NetworkRequest(channelName);
        }
    }

    while (gRunning.load())
    {
        const std::uint64_t nowMs{NowEpochMs()};

        // Check for external wakeup triggers per channel.
        for (const auto &channelName : channels)
        {
            if (CheckAndConsumeWakeupTrigger(triggerDir, channelName))
            {
                (void)manager.NmMessageIndication(channelName);
            }
        }

        manager.Tick(nowMs);
        WriteStatus(statusFile, manager);

        const std::uint32_t sleepStepMs{
            periodMs < 50U ? periodMs : 50U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    // Release all channels before exit.
    for (const auto &channelName : channels)
    {
        (void)manager.NetworkRelease(channelName);
    }

    const std::uint64_t finalNow{NowEpochMs()};
    manager.Tick(finalNow);
    WriteStatus(statusFile, manager);

    return 0;
}
