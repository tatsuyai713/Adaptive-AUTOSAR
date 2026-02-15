/// @file src/main_can_interface_manager.cpp
/// @brief Resident daemon that keeps the SocketCAN interface configured.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/types.h>

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

    bool InterfaceExists(const std::string &ifname)
    {
        const std::string path{"/sys/class/net/" + ifname + "/operstate"};
        std::ifstream stream(path);
        return stream.is_open();
    }

    bool IsInterfaceUp(const std::string &ifname)
    {
        const std::string path{"/sys/class/net/" + ifname + "/operstate"};
        std::ifstream stream(path);
        if (!stream.is_open())
        {
            return false;
        }

        std::string state;
        stream >> state;
        return state == "up" || state == "unknown";
    }

    int RunCommand(const std::string &command)
    {
        return std::system(command.c_str());
    }

    bool ReconfigureCan(
        const std::string &ifname,
        const std::string &bitrate)
    {
        const std::string downCmd{
            "ip link set " + ifname + " down >/dev/null 2>&1 || true"};
        const std::string typeCmd{
            "ip link set " + ifname + " type can bitrate " + bitrate + " >/dev/null 2>&1"};
        const std::string upCmd{
            "ip link set " + ifname + " up >/dev/null 2>&1"};

        RunCommand(downCmd);
        if (RunCommand(typeCmd) != 0)
        {
            return false;
        }
        return RunCommand(upCmd) == 0;
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    void WriteStatus(
        const std::string &statusFile,
        const std::string &ifname,
        bool exists,
        bool up,
        std::size_t reconfigSuccessCount,
        std::size_t reconfigFailureCount)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "interface=" << ifname << "\n";
        stream << "exists=" << (exists ? "true" : "false") << "\n";
        stream << "up=" << (up ? "true" : "false") << "\n";
        stream << "reconfig_success=" << reconfigSuccessCount << "\n";
        stream << "reconfig_failure=" << reconfigFailureCount << "\n";
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

    const std::string ifname{
        GetEnvOrDefault("AUTOSAR_CAN_IFNAME", "can0")};
    const std::string bitrate{
        GetEnvOrDefault("AUTOSAR_CAN_BITRATE", "500000")};
    const std::uint32_t monitorPeriodMs{
        GetEnvU32("AUTOSAR_CAN_MONITOR_PERIOD_MS", 2000U)};
    const bool reconfigureOnDown{
        GetEnvBool("AUTOSAR_CAN_RECONFIGURE_ON_DOWN", true)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_CAN_MANAGER_STATUS_FILE",
            "/run/autosar/can_manager.status")};

    EnsureRunDirectory();

    std::size_t reconfigSuccessCount{0U};
    std::size_t reconfigFailureCount{0U};

    while (gRunning.load())
    {
        const bool exists{InterfaceExists(ifname)};
        bool up{exists && IsInterfaceUp(ifname)};

        if (exists && !up && reconfigureOnDown)
        {
            if (ReconfigureCan(ifname, bitrate))
            {
                ++reconfigSuccessCount;
            }
            else
            {
                ++reconfigFailureCount;
            }
            up = IsInterfaceUp(ifname);
        }

        WriteStatus(
            statusFile,
            ifname,
            exists,
            up,
            reconfigSuccessCount,
            reconfigFailureCount);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < monitorPeriodMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    return 0;
}
