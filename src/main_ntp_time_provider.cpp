/// @file src/main_ntp_time_provider.cpp
/// @brief Resident daemon that synchronizes time via NTP (chrony/ntpd).
/// @details Periodically queries the local NTP daemon for clock offset
///          and updates a TimeSyncClient with the corrected reference time.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/tsync/ntp_time_base_provider.h"
#include "./ara/tsync/time_sync_client.h"

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
            if (parsed == 0U || parsed > 3600000U)
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

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    const char *DaemonToString(ara::tsync::NtpDaemon daemon)
    {
        switch (daemon)
        {
        case ara::tsync::NtpDaemon::kChrony:
            return "chrony";
        case ara::tsync::NtpDaemon::kNtpd:
            return "ntpd";
        case ara::tsync::NtpDaemon::kAuto:
            return "none";
        default:
            return "unknown";
        }
    }

    ara::tsync::NtpDaemon ParseDaemonType(const std::string &value)
    {
        if (value == "chrony")
        {
            return ara::tsync::NtpDaemon::kChrony;
        }
        if (value == "ntpd")
        {
            return ara::tsync::NtpDaemon::kNtpd;
        }
        return ara::tsync::NtpDaemon::kAuto;
    }

    void WriteStatusFile(
        const std::string &statusFile,
        const ara::tsync::NtpTimeBaseProvider &provider,
        const ara::tsync::TimeSyncClient &client)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        const bool available{provider.IsSourceAvailable()};
        const bool synchronized{
            client.GetState() ==
            ara::tsync::SynchronizationState::kSynchronized};

        stream << "provider=" << provider.GetProviderName() << "\n";
        stream << "daemon=" << DaemonToString(provider.GetActiveDaemon())
               << "\n";
        stream << "source_available=" << (available ? "true" : "false")
               << "\n";
        stream << "synchronized=" << (synchronized ? "true" : "false")
               << "\n";

        const auto offsetResult{client.GetCurrentOffset()};
        if (offsetResult.HasValue())
        {
            stream << "offset_ns=" << offsetResult.Value().count() << "\n";
        }
        else
        {
            stream << "offset_ns=unavailable\n";
        }

        const auto nowMs{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()};
        stream << "updated_epoch_ms=" << nowMs << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_NTP_PERIOD_MS", 1000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_NTP_STATUS_FILE",
            "/run/autosar/ntp_time_provider.status")};
    const std::string daemonStr{
        GetEnvOrDefault("AUTOSAR_NTP_DAEMON", "auto")};

    EnsureRunDirectory();

    const ara::tsync::NtpDaemon daemonType{ParseDaemonType(daemonStr)};
    ara::tsync::NtpTimeBaseProvider provider{daemonType};
    ara::tsync::TimeSyncClient client;

    while (gRunning.load())
    {
        provider.UpdateTimeBase(client);
        WriteStatusFile(statusFile, provider, client);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    return 0;
}
