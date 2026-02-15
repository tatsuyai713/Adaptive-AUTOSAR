/// @file src/main_time_sync_daemon.cpp
/// @brief Resident daemon that tracks time synchronization status.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>

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

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    void WriteStatusFile(
        const std::string &statusFile,
        const ara::tsync::TimeSyncClient &client)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        const auto offsetResult{client.GetCurrentOffset()};
        const bool synchronized{
            client.GetState() == ara::tsync::SynchronizationState::kSynchronized};

        stream << "synchronized=" << (synchronized ? "true" : "false") << "\n";
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
        GetEnvU32("AUTOSAR_TIMESYNC_PERIOD_MS", 1000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_TIMESYNC_STATUS_FILE",
            "/run/autosar/time_sync.status")};

    EnsureRunDirectory();

    ara::tsync::TimeSyncClient client;

    while (gRunning.load())
    {
        client.UpdateReferenceTime(
            std::chrono::system_clock::now(),
            std::chrono::steady_clock::now());
        WriteStatusFile(statusFile, client);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    return 0;
}
