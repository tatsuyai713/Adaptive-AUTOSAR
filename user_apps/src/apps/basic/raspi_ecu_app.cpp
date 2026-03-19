/// @file user_apps/src/apps/basic/raspi_ecu_app.cpp
/// @brief Raspberry Pi ECU application — production-style AUTOSAR AP vehicle
///        application that delegates platform concerns to dedicated daemons.
/// @details Unlike the mock variant, this binary acts as a thin vehicle
///          application on a real (or virtual-CAN) ECU.  Platform services
///          — diagnostic server, PHM orchestrator, crypto provider, UCM,
///          NM, time-sync — all run as separate systemd daemons.
///
///          This application is responsible for:
///            (1) Lifecycle management (ara::exec)
///            (2) Health reporting to PHM daemon (ara::phm)
///            (3) Persistent counters / config (ara::per)
///            (4) Reading vehicle sensor data from CAN / vCAN
///            (5) Publishing fused telemetry via DLT logging & status file
///            (6) Reacting to machine-state transitions (ara::sm)
///
///          All other platform functionality is handled by the daemons:
///            autosar_diag_server, autosar_phm_daemon, autosar_crypto_provider,
///            autosar_ucm_daemon, autosar_network_manager,
///            autosar_time_sync_daemon, etc.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include "ara/core/initialization.h"
#include "ara/core/instance_specifier.h"

#include "ara/log/logging_framework.h"

#include "ara/exec/application_client.h"
#include "ara/exec/signal_handler.h"

#include "ara/phm/health_channel.h"

#include "ara/per/persistency.h"

#include "ara/sm/machine_state_client.h"

// ============================================================
// Configuration
// ============================================================
namespace
{
    struct EcuConfig
    {
        /// Main loop period in milliseconds.
        std::uint32_t PeriodMs{100U};
        /// Log a status line every N cycles.
        std::uint32_t LogEvery{50U};
        /// Sync persistent counters every N cycles.
        std::uint32_t PersistEvery{100U};
        /// Path for status file.
        std::string StatusFile{"/run/autosar/ecu_app.status"};
        /// vCAN / CAN interface name (for future CAN reading).
        std::string CanInterface{"vcan0"};
    };

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

    EcuConfig LoadConfig()
    {
        EcuConfig cfg;
        cfg.PeriodMs = GetEnvU32("AUTOSAR_ECU_PERIOD_MS", cfg.PeriodMs);
        cfg.LogEvery = GetEnvU32("AUTOSAR_ECU_LOG_EVERY", cfg.LogEvery);
        cfg.PersistEvery = GetEnvU32("AUTOSAR_ECU_PERSIST_EVERY", cfg.PersistEvery);
        cfg.StatusFile = GetEnvOrDefault("AUTOSAR_ECU_STATUS_FILE", cfg.StatusFile);
        cfg.CanInterface = GetEnvOrDefault("AUTOSAR_ECU_CAN_IFNAME", cfg.CanInterface);
        return cfg;
    }

    ara::core::InstanceSpecifier MakeSpecifier(const std::string &path)
    {
        auto result{ara::core::InstanceSpecifier::Create(path)};
        if (!result.HasValue())
        {
            throw std::runtime_error(
                std::string("Bad instance specifier '") + path + "': " +
                result.Error().Message());
        }
        return result.Value();
    }

    std::uint64_t NowEpochMs()
    {
        using namespace std::chrono;
        return static_cast<std::uint64_t>(
            duration_cast<milliseconds>(
                system_clock::now().time_since_epoch())
                .count());
    }

    const char *MachineStateStr(ara::sm::MachineState state) noexcept
    {
        switch (state)
        {
        case ara::sm::MachineState::kStartup:    return "Startup";
        case ara::sm::MachineState::kRunning:     return "Running";
        case ara::sm::MachineState::kShutdown:    return "Shutdown";
        case ara::sm::MachineState::kRestart:     return "Restart";
        case ara::sm::MachineState::kSuspend:     return "Suspend";
        case ara::sm::MachineState::kDiagnostic:  return "Diagnostic";
        case ara::sm::MachineState::kUpdate:      return "Update";
        default:                                   return "Unknown";
        }
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    // Simulated vehicle sensor data (in production, read from CAN bus).
    struct VehicleSensorData
    {
        std::uint32_t SpeedCentiKph{0U};
        std::uint32_t EngineRpm{0U};
        std::int32_t SteeringAngleCentiDeg{0};
        std::uint8_t Gear{0U};
        std::uint8_t StatusFlags{0U};
        float CoolantTempC{90.0F};
        float BatteryVoltage{12.6F};
    };

    // Read vehicle sensors.  On a real ECU this reads from CAN bus sockets.
    // For development with vCAN, we generate plausible synthetic data.
    VehicleSensorData ReadVehicleSensors(std::uint32_t cycle)
    {
        VehicleSensorData data;
        // Generate varying but deterministic sensor values.
        data.SpeedCentiKph = static_cast<std::uint32_t>(
            6000U + (cycle % 200U) * 50U);  // 60.00 – 159.50 km/h
        data.EngineRpm = 2000U + (cycle % 100U) * 30U;
        data.SteeringAngleCentiDeg = static_cast<std::int32_t>(
            (cycle % 60U) * 100) - 3000;
        data.Gear = static_cast<std::uint8_t>(3U + (cycle / 100U) % 4U);
        data.StatusFlags = static_cast<std::uint8_t>(
            0x01U | ((cycle % 10U == 0U) ? 0x02U : 0x00U));
        data.CoolantTempC = 85.0F + static_cast<float>(cycle % 20U) * 0.5F;
        data.BatteryVoltage = 12.0F + static_cast<float>(cycle % 8U) * 0.1F;
        return data;
    }

    void WriteStatus(
        const std::string &path,
        std::uint32_t cycle,
        std::uint64_t bootCount,
        const char *machineState,
        std::uint32_t healthReports,
        const VehicleSensorData &sensors)
    {
        std::ofstream stream(path);
        if (!stream.is_open())
        {
            return;
        }
        stream << "cycle=" << cycle << "\n"
               << "boot_count=" << bootCount << "\n"
               << "machine_state=" << machineState << "\n"
               << "health_reports=" << healthReports << "\n"
               << "speed_centi_kph=" << sensors.SpeedCentiKph << "\n"
               << "engine_rpm=" << sensors.EngineRpm << "\n"
               << "steering_centi_deg=" << sensors.SteeringAngleCentiDeg << "\n"
               << "gear=" << static_cast<unsigned>(sensors.Gear) << "\n"
               << "coolant_temp_c=" << sensors.CoolantTempC << "\n"
               << "battery_voltage=" << sensors.BatteryVoltage << "\n"
               << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

// ============================================================
// Main
// ============================================================
int main()
{
    const EcuConfig cfg{LoadConfig()};

    // 1. Initialize AUTOSAR runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[RaspiEcu] ara::core::Initialize failed: "
                  << initResult.Error().Message() << "\n";
        return 1;
    }
    ara::exec::SignalHandler::Register();

    // 2. Logging.
    auto logging{
        std::unique_ptr<ara::log::LoggingFramework>{
            ara::log::LoggingFramework::Create(
                "RECU",
                ara::log::LogMode::kConsole,
                ara::log::LogLevel::kInfo,
                "Raspberry Pi ECU Application")}};

    const auto &logger{
        logging->CreateLogger(
            "MAIN",
            "ECU main task",
            ara::log::LogLevel::kInfo)};

    auto Log = [&](ara::log::LogLevel lvl, const std::string &msg)
    {
        auto stream{logger.WithLevel(lvl)};
        stream << msg;
        logging->Log(logger, lvl, stream);
    };

    Log(ara::log::LogLevel::kInfo, "=== Raspberry Pi ECU Application starting ===");

    // 3. Lifecycle management — register with Execution Management daemon.
    auto appSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiEcu/App")};
    ara::exec::ApplicationClient appClient{appSpecifier};

    appClient.SetRecoveryActionHandler(
        [&Log](ara::exec::ApplicationRecoveryAction action)
        {
            const char *name{"Unknown"};
            switch (action)
            {
            case ara::exec::ApplicationRecoveryAction::kRestart:
                name = "Restart";
                break;
            case ara::exec::ApplicationRecoveryAction::kTerminate:
                name = "Terminate";
                break;
            default:
                break;
            }
            Log(ara::log::LogLevel::kWarn,
                std::string("Recovery action from EM: ") + name);
        });

    appClient.ReportApplicationHealth();
    Log(ara::log::LogLevel::kInfo,
        "Registered with EM, instance=" + appClient.GetInstanceId());

    // 4. Health reporting to PHM daemon.
    auto phmSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiEcu/Health")};
    ara::phm::HealthChannel healthChannel{phmSpecifier};
    (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kOk);
    Log(ara::log::LogLevel::kInfo, "Health channel active, reported Ok to PHM daemon");

    // 5. Persistent counters.
    ara::per::SharedHandle<ara::per::KeyValueStorage> storage;
    {
        auto perSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiEcu/Storage")};
        auto storageResult{ara::per::OpenKeyValueStorage(perSpecifier)};
        if (storageResult.HasValue())
        {
            storage = storageResult.Value();
            Log(ara::log::LogLevel::kInfo, "Persistent storage opened");
        }
        else
        {
            Log(ara::log::LogLevel::kWarn,
                "Persistency unavailable (guard daemon may not be running): " +
                storageResult.Error().Message());
        }
    }

    auto LoadCounter = [&](const std::string &key) -> std::uint64_t
    {
        if (!storage)
        {
            return 0U;
        }
        auto r{storage->GetValue<std::uint64_t>(key)};
        return r.HasValue() ? r.Value() : 0U;
    };
    auto SaveCounter = [&](const std::string &key, std::uint64_t val)
    {
        if (!storage)
        {
            return;
        }
        (void)storage->SetValue<std::uint64_t>(key, val);
    };

    std::uint64_t bootCount{LoadCounter("raspi_ecu.boot_count") + 1U};
    std::uint64_t cycleCount{LoadCounter("raspi_ecu.cycle_count")};
    SaveCounter("raspi_ecu.boot_count", bootCount);
    if (storage)
    {
        (void)storage->SyncToStorage();
    }

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Boot #" << static_cast<std::uint32_t>(bootCount)
               << ", previous cycles: " << static_cast<std::uint32_t>(cycleCount);
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 6. Machine state — react to state transitions from SM daemon.
    ara::sm::MachineStateClient machineState;
    machineState.SetNotifier(
        [&Log, &healthChannel](ara::sm::MachineState newState)
        {
            Log(ara::log::LogLevel::kInfo,
                std::string("MachineState -> ") + MachineStateStr(newState));

            if (newState == ara::sm::MachineState::kDiagnostic ||
                newState == ara::sm::MachineState::kUpdate)
            {
                (void)healthChannel.ReportHealthStatus(
                    ara::phm::HealthStatus::kOk);
            }
        });

    (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
    Log(ara::log::LogLevel::kInfo,
        std::string("SM: ") + MachineStateStr(
            machineState.GetMachineState().Value()));

    EnsureRunDirectory();

    // ================================================================
    // Main ECU processing loop
    // ================================================================
    Log(ara::log::LogLevel::kInfo, "=== Main loop started ===");

    std::uint32_t loopCycle{0U};
    auto nextTick{std::chrono::steady_clock::now()};

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // (a) Report liveness to EM and PHM.
        appClient.ReportApplicationHealth();
        (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kOk);

        // (b) Read vehicle sensors (CAN/vCAN in production, synthetic now).
        const VehicleSensorData sensors{ReadVehicleSensors(loopCycle)};

        // (c) Write status file for monitoring / other services to consume.
        if ((loopCycle % 10U) == 0U)
        {
            const auto smState{machineState.GetMachineState()};
            const char *stateStr{
                smState.HasValue() ? MachineStateStr(smState.Value()) : "?"};
            WriteStatus(
                cfg.StatusFile, loopCycle, bootCount,
                stateStr,
                static_cast<std::uint32_t>(appClient.GetHealthReportCount()),
                sensors);
        }

        // (d) Periodic logging.
        if (cfg.LogEvery > 0U && (loopCycle % cfg.LogEvery) == 0U)
        {
            const auto smState{machineState.GetMachineState()};
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "[cycle=" << loopCycle
                   << "] sm=" << (smState.HasValue()
                                      ? MachineStateStr(smState.Value())
                                      : "?")
                   << " speed=" << sensors.SpeedCentiKph
                   << " rpm=" << sensors.EngineRpm
                   << " gear=" << static_cast<unsigned>(sensors.Gear)
                   << " health_rpt="
                   << static_cast<std::uint32_t>(
                          appClient.GetHealthReportCount());
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        // (e) Persist counters periodically.
        ++cycleCount;
        if (cfg.PersistEvery > 0U && (loopCycle % cfg.PersistEvery) == 0U)
        {
            SaveCounter("raspi_ecu.cycle_count", cycleCount);
            if (storage)
            {
                (void)storage->SyncToStorage();
            }
        }

        ++loopCycle;

        // (f) Rate limiting.
        nextTick += std::chrono::milliseconds{cfg.PeriodMs};
        const auto now{std::chrono::steady_clock::now()};
        if (nextTick > now)
        {
            std::this_thread::sleep_until(nextTick);
        }
        else
        {
            nextTick = now + std::chrono::milliseconds{cfg.PeriodMs};
        }
    }

    // ================================================================
    // Graceful shutdown
    // ================================================================
    Log(ara::log::LogLevel::kInfo, "=== Graceful shutdown ===");

    SaveCounter("raspi_ecu.cycle_count", cycleCount);
    if (storage)
    {
        (void)storage->SyncToStorage();
    }

    (void)machineState.SetMachineState(ara::sm::MachineState::kShutdown);
    (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kDeactivated);

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Final: cycles=" << loopCycle
               << " boot=" << static_cast<std::uint32_t>(bootCount)
               << " health_reports="
               << static_cast<std::uint32_t>(appClient.GetHealthReportCount());
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    Log(ara::log::LogLevel::kInfo, "=== Raspberry Pi ECU Application stopped ===");
    (void)ara::core::Deinitialize();
    return 0;
}
