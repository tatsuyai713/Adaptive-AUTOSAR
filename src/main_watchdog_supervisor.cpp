/// @file src/main_watchdog_supervisor.cpp
/// @brief Resident daemon that feeds Linux watchdog or emits soft heartbeat.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
            if (parsed == 0U || parsed > 60000U)
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

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    void WriteHeartbeatFile(const std::string &heartbeatFile, bool hardwareMode)
    {
        std::ofstream stream(heartbeatFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "mode=" << (hardwareMode ? "hardware" : "soft") << "\n";
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

    const std::string watchdogDevice{
        GetEnvOrDefault("AUTOSAR_WATCHDOG_DEVICE", "/dev/watchdog")};
    const std::uint32_t intervalMs{
        GetEnvU32("AUTOSAR_WATCHDOG_INTERVAL_MS", 1000U)};
    const bool allowSoftMode{
        GetEnvBool("AUTOSAR_WATCHDOG_ALLOW_SOFT_MODE", true)};
    const std::string heartbeatFile{
        GetEnvOrDefault(
            "AUTOSAR_WATCHDOG_HEARTBEAT_FILE",
            "/run/autosar/watchdog.status")};

    EnsureRunDirectory();

    int watchdogFd{-1};
    bool hardwareMode{false};

    watchdogFd = ::open(watchdogDevice.c_str(), O_WRONLY | O_CLOEXEC);
    if (watchdogFd >= 0)
    {
        hardwareMode = true;
    }
    else if (!allowSoftMode)
    {
        return 1;
    }

    while (gRunning.load())
    {
        if (hardwareMode)
        {
            const std::uint8_t keepalive{0U};
            const ssize_t written{
                ::write(watchdogFd, &keepalive, sizeof(keepalive))};
            if (written < 0)
            {
                hardwareMode = false;
                if (!allowSoftMode)
                {
                    ::close(watchdogFd);
                    return 1;
                }
            }
        }

        WriteHeartbeatFile(heartbeatFile, hardwareMode);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < intervalMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    if (watchdogFd >= 0)
    {
        // Magic close sequence for Linux watchdog drivers.
        const char magicClose{'V'};
        (void)::write(watchdogFd, &magicClose, sizeof(magicClose));
        ::close(watchdogFd);
    }

    return 0;
}
