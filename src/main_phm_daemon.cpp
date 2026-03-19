/// @file src/main_phm_daemon.cpp
/// @brief Resident daemon that runs the AUTOSAR Platform Health Management
///        (PHM) orchestrator, monitoring registered supervised entities and
///        dispatching recovery actions.
/// @details The daemon aggregates supervision results from all registered
///          entities, evaluates platform-wide health, and writes periodic
///          status to /run/autosar/phm.status.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/core/initialization.h"
#include "./ara/phm/phm_orchestrator.h"
#include "./ara/phm/health_channel.h"
#include "./ara/phm/alive_supervision.h"
#include "./ara/phm/deadline_supervision.h"
#include "./ara/phm/logical_supervision.h"

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
            if (parsed > 600000U)
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

    double GetEnvDouble(const char *key, double fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            return std::stod(value);
        }
        catch (...)
        {
            return fallback;
        }
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

    const char *PlatformHealthToString(
        ara::phm::PlatformHealthState state)
    {
        switch (state)
        {
        case ara::phm::PlatformHealthState::kNormal:
            return "Normal";
        case ara::phm::PlatformHealthState::kDegraded:
            return "Degraded";
        case ara::phm::PlatformHealthState::kCritical:
            return "Critical";
        default:
            return "Unknown";
        }
    }

    const char *SupervisionStatusToString(
        ara::phm::GlobalSupervisionStatus status)
    {
        switch (status)
        {
        case ara::phm::GlobalSupervisionStatus::kOk:
            return "Ok";
        case ara::phm::GlobalSupervisionStatus::kFailed:
            return "Failed";
        case ara::phm::GlobalSupervisionStatus::kExpired:
            return "Expired";
        case ara::phm::GlobalSupervisionStatus::kDeactivated:
            return "Deactivated";
        default:
            return "Unknown";
        }
    }

    void WriteStatusFile(
        const std::string &statusFile,
        const ara::phm::PhmOrchestrator &orchestrator)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        const auto platformState{orchestrator.EvaluatePlatformHealth()};
        stream << "platform_health="
               << PlatformHealthToString(platformState) << "\n";
        stream << "entity_count="
               << orchestrator.GetEntityCount() << "\n";

        const auto entities{orchestrator.GetAllEntityHealth()};
        std::size_t failedCount{0U};
        for (const auto &entity : entities)
        {
            stream << "entity." << entity.EntityName << ".status="
                   << SupervisionStatusToString(entity.Status) << "\n";
            stream << "entity." << entity.EntityName << ".failures="
                   << entity.FailureCount << "\n";
            if (entity.Status == ara::phm::GlobalSupervisionStatus::kFailed ||
                entity.Status == ara::phm::GlobalSupervisionStatus::kExpired)
            {
                ++failedCount;
            }
        }

        stream << "failed_entity_count=" << failedCount << "\n";
        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_PHM_PERIOD_MS", 1000U)};
    const double degradedThreshold{
        GetEnvDouble("AUTOSAR_PHM_DEGRADED_THRESHOLD", 0.0)};
    const double criticalThreshold{
        GetEnvDouble("AUTOSAR_PHM_CRITICAL_THRESHOLD", 0.5)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_PHM_STATUS_FILE",
            "/run/autosar/phm.status")};

    // Predefined entity names to register (matching daemon/service names).
    const std::string entityList{
        GetEnvOrDefault(
            "AUTOSAR_PHM_ENTITIES",
            "platform_app,vsomeip_routing,time_sync,persistency_guard,"
            "iam_policy,can_manager,watchdog,network_manager,ucm,"
            "dlt,diag_server,crypto_provider,ecu_app")};

    EnsureRunDirectory();

    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        return 1;
    }

    ara::phm::PhmOrchestrator orchestrator{
        degradedThreshold, criticalThreshold};

    orchestrator.SetPlatformHealthCallback(
        [](ara::phm::PlatformHealthState,
           ara::phm::PlatformHealthState)
        {
            // Transition logged via DLT in production.
        });

    // Parse comma-separated entity list and register each.
    {
        std::string remaining{entityList};
        while (!remaining.empty())
        {
            const auto delim{remaining.find(',')};
            std::string name;
            if (delim == std::string::npos)
            {
                name = remaining;
                remaining.clear();
            }
            else
            {
                name = remaining.substr(0U, delim);
                remaining = remaining.substr(delim + 1U);
            }

            // Trim whitespace.
            while (!name.empty() && name.front() == ' ')
            {
                name.erase(name.begin());
            }
            while (!name.empty() && name.back() == ' ')
            {
                name.pop_back();
            }

            if (!name.empty())
            {
                (void)orchestrator.RegisterEntity(name);
            }
        }
    }

    // Main monitoring loop.
    while (gRunning.load())
    {
        // Evaluate current platform health (updates internal state).
        (void)orchestrator.EvaluatePlatformHealth();

        // Write status file.
        WriteStatusFile(statusFile, orchestrator);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    // Final status write.
    WriteStatusFile(statusFile, orchestrator);

    (void)ara::core::Deinitialize();
    return 0;
}
