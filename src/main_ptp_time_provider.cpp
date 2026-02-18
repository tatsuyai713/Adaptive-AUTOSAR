/// @file src/main_ptp_time_provider.cpp
/// @brief Resident daemon that synchronizes time via PTP/gPTP (ptp4l).
/// @details Periodically reads the PTP Hardware Clock (PHC) at /dev/ptpN
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

#include "./ara/tsync/ptp_time_base_provider.h"
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
#if defined(__QNX__)
        ::mkdir("/tmp/autosar", 0755);
#else
        ::mkdir("/run/autosar", 0755);
#endif
    }

    void WriteStatusFile(
        const std::string &statusFile,
        const ara::tsync::PtpTimeBaseProvider &provider,
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
        stream << "device=" << provider.GetDevicePath() << "\n";
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
        GetEnvU32("AUTOSAR_PTP_PERIOD_MS", 500U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_PTP_STATUS_FILE",
#if defined(__QNX__)
            "/tmp/autosar/ptp_time_provider.status"
#else
            "/run/autosar/ptp_time_provider.status"
#endif
        )};
    const std::string ptpDevice{
        GetEnvOrDefault("AUTOSAR_PTP_DEVICE", "/dev/ptp0")};

    EnsureRunDirectory();

    ara::tsync::PtpTimeBaseProvider provider{ptpDevice};
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
