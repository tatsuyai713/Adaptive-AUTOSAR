/// @file user_apps/src/apps/basic/raspi_mock_ecu_app.cpp
/// @brief Raspberry Pi mock ECU — full-stack AUTOSAR Adaptive Platform demo.
/// @details This application exercises all 11 ara::* functional clusters
///          without requiring any external middleware (vsomeip, DDS, CAN
///          hardware).  It is designed to run on Raspberry Pi (Linux ARM)
///          and any standard Linux/QNX workstation.
///
/// Covered modules
///   ara::core   — Initialize/Deinitialize, InstanceSpecifier
///   ara::log    — LoggingFramework, console sink
///   ara::exec   — SignalHandler, ApplicationClient
///   ara::phm    — HealthChannel
///   ara::per    — KeyValueStorage
///   ara::diag   — DiagnosticManager (UDS service simulation)
///   ara::sm     — MachineStateClient
///   ara::tsync  — TimeSyncClient (mock NTP samples)
///   ara::nm     — NetworkManager (CAN NM channel lifecycle)
///   ara::crypto — HsmProvider (AES-128 key, encrypt/decrypt/HMAC)
///   ara::ucm    — UpdateManager (staged software update flow)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// OpenSSL SHA-256 for UCM package digest computation.
#include <openssl/sha.h>

// AUTOSAR Adaptive Platform headers
#include "ara/core/initialization.h"
#include "ara/core/instance_specifier.h"

#include "ara/log/logging_framework.h"

#include "ara/exec/application_client.h"
#include "ara/exec/signal_handler.h"

#include "ara/phm/health_channel.h"

#include "ara/per/persistency.h"

#include "ara/diag/diagnostic_manager.h"

#include "ara/sm/machine_state_client.h"

#include "ara/tsync/time_sync_client.h"

#include "ara/nm/network_manager.h"

#include "ara/crypto/hsm_provider.h"

#include "ara/ucm/update_manager.h"

// ============================================================
// Configuration
// ============================================================
namespace
{
    struct AppConfig
    {
        /// Total simulation cycles to run (0 = run until signal).
        std::uint32_t MaxCycles{0U};
        /// Loop period in milliseconds.
        std::uint32_t PeriodMs{500U};
        /// Log a status line every N cycles.
        std::uint32_t LogEvery{10U};
        /// Run one software-update simulation every N cycles (0 = once at start).
        std::uint32_t UcmEvery{40U};
        /// Verbose crypto output.
        bool VerboseCrypto{false};
    };

    AppConfig ParseArgs(int argc, char *argv[])
    {
        AppConfig cfg;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg{argv[i]};

            auto splitAt = [&](const std::string &prefix) -> std::string
            {
                if (arg.find(prefix + "=") == 0U)
                {
                    return arg.substr(prefix.size() + 1U);
                }
                return {};
            };

            std::string val;
            val = splitAt("--max-cycles");
            if (!val.empty())
            {
                cfg.MaxCycles = static_cast<std::uint32_t>(std::stoul(val));
            }
            val = splitAt("--period-ms");
            if (!val.empty())
            {
                cfg.PeriodMs = static_cast<std::uint32_t>(std::stoul(val));
            }
            val = splitAt("--log-every");
            if (!val.empty())
            {
                cfg.LogEvery = static_cast<std::uint32_t>(std::stoul(val));
            }
            val = splitAt("--ucm-every");
            if (!val.empty())
            {
                cfg.UcmEvery = static_cast<std::uint32_t>(std::stoul(val));
            }
            if (arg == "--verbose-crypto")
            {
                cfg.VerboseCrypto = true;
            }
        }
        return cfg;
    }

    // ---- Helpers -------------------------------------------------------

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

    // Convert MachineState enum to descriptive string.
    const char *MachineStateStr(ara::sm::MachineState state) noexcept
    {
        switch (state)
        {
        case ara::sm::MachineState::kStartup:    return "Startup";
        case ara::sm::MachineState::kRunning:    return "Running";
        case ara::sm::MachineState::kShutdown:   return "Shutdown";
        case ara::sm::MachineState::kRestart:    return "Restart";
        case ara::sm::MachineState::kSuspend:    return "Suspend";
        case ara::sm::MachineState::kDiagnostic: return "Diagnostic";
        case ara::sm::MachineState::kUpdate:     return "Update";
        default:                                  return "Unknown";
        }
    }

    // Convert NmState enum to string.
    const char *NmStateStr(ara::nm::NmState s) noexcept
    {
        switch (s)
        {
        case ara::nm::NmState::kBusSleep:         return "BusSleep";
        case ara::nm::NmState::kPrepBusSleep:     return "PrepBusSleep";
        case ara::nm::NmState::kReadySleep:       return "ReadySleep";
        case ara::nm::NmState::kNormalOperation:  return "NormalOperation";
        case ara::nm::NmState::kRepeatMessage:    return "RepeatMessage";
        default:                                   return "Unknown";
        }
    }

    // Convert SyncState enum to string.
    const char *SyncStateStr(ara::tsync::SynchronizationState s) noexcept
    {
        switch (s)
        {
        case ara::tsync::SynchronizationState::kSynchronized:   return "Synchronized";
        case ara::tsync::SynchronizationState::kUnsynchronized: return "Unsynchronized";
        default:                                                   return "Unknown";
        }
    }
}

// ============================================================
// Main
// ============================================================
int main(int argc, char *argv[])
{
    const AppConfig cfg{ParseArgs(argc, argv)};

    // ------------------------------------------------------------------
    // 1. ara::core  — Initialize runtime
    // ------------------------------------------------------------------
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[RaspiMockEcu] ara::core::Initialize failed: "
                  << initResult.Error().Message() << "\n";
        return 1;
    }

    ara::exec::SignalHandler::Register();

    // ------------------------------------------------------------------
    // 2. ara::log  — Set up console logging
    // ------------------------------------------------------------------
    auto logging{
        std::unique_ptr<ara::log::LoggingFramework>{
            ara::log::LoggingFramework::Create(
                "RMEC",
                ara::log::LogMode::kConsole,
                ara::log::LogLevel::kInfo,
                "Raspberry Pi Mock ECU")}};

    const auto &logger{
        logging->CreateLogger(
            "MAIN",
            "Main ECU task",
            ara::log::LogLevel::kInfo)};

    auto Log = [&](ara::log::LogLevel lvl, const std::string &msg)
    {
        auto stream{logger.WithLevel(lvl)};
        stream << msg;
        logging->Log(logger, lvl, stream);
    };

    Log(ara::log::LogLevel::kInfo, "=== Raspberry Pi Mock ECU starting ===");

    // ------------------------------------------------------------------
    // 3. ara::exec  — ApplicationClient (lifecycle management)
    // ------------------------------------------------------------------
    auto appSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiMockEcu/App")};
    ara::exec::ApplicationClient appClient{appSpecifier};

    appClient.SetRecoveryActionHandler(
        [&Log](ara::exec::ApplicationRecoveryAction action)
        {
            switch (action)
            {
            case ara::exec::ApplicationRecoveryAction::kRestart:
                Log(ara::log::LogLevel::kWarn, "Recovery action: Restart requested");
                break;
            case ara::exec::ApplicationRecoveryAction::kTerminate:
                Log(ara::log::LogLevel::kWarn, "Recovery action: Terminate requested");
                break;
            default:
                break;
            }
        });

    appClient.ReportApplicationHealth();
    Log(ara::log::LogLevel::kInfo,
        "ApplicationClient started, instance=" + appClient.GetInstanceId());

    // ------------------------------------------------------------------
    // 4. ara::phm  — HealthChannel
    // ------------------------------------------------------------------
    auto phmSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiMockEcu/Health")};
    ara::phm::HealthChannel healthChannel{phmSpecifier};
    (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kOk);
    Log(ara::log::LogLevel::kInfo, "PHM HealthChannel reported Ok");

    // ------------------------------------------------------------------
    // 5. ara::per  — KeyValueStorage (persistent counters)
    // ------------------------------------------------------------------
    ara::per::SharedHandle<ara::per::KeyValueStorage> storage;
    {
        auto perSpecifier{MakeSpecifier("AdaptiveAutosar/UserApps/RaspiMockEcu/Storage")};
        auto storageResult{ara::per::OpenKeyValueStorage(perSpecifier)};
        if (storageResult.HasValue())
        {
            storage = storageResult.Value();
            Log(ara::log::LogLevel::kInfo, "PER KeyValueStorage opened");
        }
        else
        {
            Log(ara::log::LogLevel::kWarn,
                "PER storage unavailable: " + storageResult.Error().Message());
        }
    }

    // Load persistent counters (survive restarts).
    auto LoadCounter = [&](const std::string &key) -> std::uint64_t
    {
        if (!storage) { return 0U; }
        auto r{storage->GetValue<std::uint64_t>(key)};
        return r.HasValue() ? r.Value() : 0U;
    };
    auto SaveCounter = [&](const std::string &key, std::uint64_t val)
    {
        if (!storage) { return; }
        (void)storage->SetValue<std::uint64_t>(key, val);
    };

    std::uint64_t bootCount{LoadCounter("raspi_ecu.boot_count") + 1U};
    std::uint64_t cycleCount{LoadCounter("raspi_ecu.cycle_count")};
    std::uint64_t diagRequestCount{LoadCounter("raspi_ecu.diag_requests")};

    SaveCounter("raspi_ecu.boot_count", bootCount);
    if (storage) { (void)storage->SyncToStorage(); }

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "PER: boot_count=" << static_cast<std::uint32_t>(bootCount)
               << " prev_cycles=" << static_cast<std::uint32_t>(cycleCount);
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // ------------------------------------------------------------------
    // 6. ara::diag  — DiagnosticManager (UDS service simulation)
    // ------------------------------------------------------------------
    ara::diag::DiagnosticManager diagMgr;
    diagMgr.SetResponseTiming({50U, 5000U});

    // Register UDS services this ECU supports.
    const std::vector<std::uint8_t> diagServices{
        0x10U, // DiagnosticSessionControl
        0x11U, // ECUReset
        0x14U, // ClearDTCInformation
        0x19U, // ReadDTCInformation
        0x22U, // ReadDataByIdentifier
        0x27U, // SecurityAccess
        0x2EU, // WriteDataByIdentifier
        0x31U, // RoutineControl
        0x3EU  // TesterPresent
    };
    for (auto svc : diagServices)
    {
        (void)diagMgr.RegisterService(svc);
    }

    diagMgr.SetResponsePendingCallback(
        [&Log](std::uint32_t reqId, std::uint8_t svcId)
        {
            auto stream{std::string("Diag response pending: req=") +
                        std::to_string(reqId) + " svc=0x" +
                        [svcId]
                        {
                            char buf[8];
                            std::snprintf(buf, sizeof(buf), "%02X", svcId);
                            return std::string(buf);
                        }()};
            Log(ara::log::LogLevel::kWarn, stream);
        });

    Log(ara::log::LogLevel::kInfo,
        "DiagnosticManager: " + std::to_string(diagServices.size()) +
        " UDS services registered");

    // ------------------------------------------------------------------
    // 7. ara::sm  — MachineStateClient
    // ------------------------------------------------------------------
    ara::sm::MachineStateClient machineState;

    machineState.SetNotifier(
        [&Log, &healthChannel](ara::sm::MachineState newState)
        {
            Log(ara::log::LogLevel::kInfo,
                std::string("MachineState -> ") + MachineStateStr(newState));

            // Update PHM when entering diagnostic or update mode.
            if (newState == ara::sm::MachineState::kDiagnostic ||
                newState == ara::sm::MachineState::kUpdate)
            {
                (void)healthChannel.ReportHealthStatus(
                    ara::phm::HealthStatus::kOk);
            }
        });

    // Transition from kStartup to kRunning.
    (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
    Log(ara::log::LogLevel::kInfo,
        std::string("MachineStateClient initialised, state=") +
        MachineStateStr(machineState.GetMachineState().Value()));

    // ------------------------------------------------------------------
    // 8. ara::tsync  — TimeSyncClient (mock NTP simulation)
    // ------------------------------------------------------------------
    ara::tsync::TimeSyncClient timeSyncClient;
    timeSyncClient.SetQualityThreshold(2000LL);       // 2 µs/s tolerance
    timeSyncClient.SetSyncLossTimeoutMs(10000U);      // 10 s loss timeout

    timeSyncClient.SetStateChangeNotifier(
        [&Log](ara::tsync::SynchronizationState newState)
        {
            Log(ara::log::LogLevel::kInfo,
                std::string("TimeSyncClient: ") + SyncStateStr(newState));
        });

    timeSyncClient.SetTimeLeapCallback(
        [&Log](std::chrono::nanoseconds leap)
        {
            auto stream{std::string("TimeLeap detected: ") +
                        std::to_string(leap.count() / 1'000'000LL) + " ms"};
            Log(ara::log::LogLevel::kWarn, stream);
        });

    // Seed initial time reference (simulates first NTP packet).
    (void)timeSyncClient.UpdateReferenceTime(
        std::chrono::system_clock::now(),
        std::chrono::steady_clock::now());

    Log(ara::log::LogLevel::kInfo,
        std::string("TimeSyncClient initialised, state=") +
        SyncStateStr(timeSyncClient.GetState()));

    // ------------------------------------------------------------------
    // 9. ara::nm  — NetworkManager (CAN NM channel lifecycle)
    // ------------------------------------------------------------------
    ara::nm::NetworkManager nmMgr;

    const std::string kCanChannel{"CAN0"};
    {
        ara::nm::NmChannelConfig canCfg;
        canCfg.ChannelName          = kCanChannel;
        canCfg.NmTimeoutMs          = 3000U;
        canCfg.RepeatMessageTimeMs  = 1000U;
        canCfg.WaitBusSleepTimeMs   = 1500U;
        canCfg.PartialNetworkEnabled = false;
        (void)nmMgr.AddChannel(canCfg);
    }

    nmMgr.SetStateChangeHandler(
        [&Log](const std::string &channel,
               ara::nm::NmState oldState,
               ara::nm::NmState newState)
        {
            Log(ara::log::LogLevel::kInfo,
                "NM[" + channel + "]: " +
                NmStateStr(oldState) + " -> " + NmStateStr(newState));
        });

    // Request the CAN network — moves channel to NormalOperation.
    (void)nmMgr.NetworkRequest(kCanChannel);
    nmMgr.Tick(NowEpochMs());

    Log(ara::log::LogLevel::kInfo, "NetworkManager: CAN0 channel active");

    // ------------------------------------------------------------------
    // 10. ara::crypto  — HsmProvider (software HSM emulation)
    // ------------------------------------------------------------------
    ara::crypto::HsmProvider hsm;
    (void)hsm.Initialize("RaspiMockEcuToken");

    // Generate an AES-128 symmetric key for payload protection.
    std::uint32_t payloadKeySlot{0U};
    {
        auto keyResult{hsm.GenerateKey(
            ara::crypto::HsmAlgorithm::kAes128, "payload_key")};
        if (keyResult.HasValue())
        {
            payloadKeySlot = keyResult.Value();
            Log(ara::log::LogLevel::kInfo,
                "HSM: AES-128 key generated, slot=" +
                std::to_string(payloadKeySlot));
        }
        else
        {
            Log(ara::log::LogLevel::kError,
                "HSM: Key generation failed: " + keyResult.Error().Message());
        }
    }

    // Generate an HMAC-SHA256 key for message authentication.
    std::uint32_t hmacKeySlot{0U};
    {
        auto keyResult{hsm.GenerateKey(
            ara::crypto::HsmAlgorithm::kHmacSha256, "hmac_key")};
        if (keyResult.HasValue())
        {
            hmacKeySlot = keyResult.Value();
            Log(ara::log::LogLevel::kInfo,
                "HSM: HMAC-SHA256 key generated, slot=" +
                std::to_string(hmacKeySlot));
        }
    }

    // Initial self-test: encrypt and decrypt a known test vector.
    bool cryptoSelfTestOk{false};
    if (payloadKeySlot > 0U)
    {
        const std::vector<std::uint8_t> testPlain{
            0x48U, 0x65U, 0x6CU, 0x6CU, 0x6FU, 0x20U,
            0x52U, 0x61U, 0x73U, 0x70U, 0x69U, 0x21U,
            0x00U, 0x00U, 0x00U, 0x00U  // 16-byte aligned
        };
        auto encResult{hsm.Encrypt(payloadKeySlot, testPlain)};
        if (encResult.Success)
        {
            auto decResult{hsm.Decrypt(payloadKeySlot, encResult.OutputData)};
            if (decResult.Success && decResult.OutputData == testPlain)
            {
                cryptoSelfTestOk = true;
                Log(ara::log::LogLevel::kInfo,
                    "HSM: AES-CBC self-test PASSED");
            }
            else
            {
                Log(ara::log::LogLevel::kError,
                    "HSM: AES-CBC self-test FAILED (decrypt mismatch)");
            }
        }
        else
        {
            Log(ara::log::LogLevel::kError,
                "HSM: AES-CBC self-test FAILED (encrypt error): " +
                encResult.ErrorMessage);
        }
    }

    // HMAC sign/verify self-test.
    bool hmacSelfTestOk{false};
    if (hmacKeySlot > 0U)
    {
        const std::vector<std::uint8_t> testMsg{
            0x01U, 0x02U, 0x03U, 0x04U, 0x05U};
        auto signResult{hsm.Sign(hmacKeySlot, testMsg)};
        if (signResult.Success)
        {
            bool verified{
                hsm.Verify(hmacKeySlot, testMsg, signResult.OutputData)};
            hmacSelfTestOk = verified;
            Log(ara::log::LogLevel::kInfo,
                std::string("HSM: HMAC self-test ") +
                (verified ? "PASSED" : "FAILED"));
        }
    }

    // ------------------------------------------------------------------
    // 11. ara::ucm  — UpdateManager (staged software update flow)
    // ------------------------------------------------------------------
    ara::ucm::UpdateManager ucmMgr;

    (void)ucmMgr.SetStateChangeHandler(
        [&Log](ara::ucm::UpdateSessionState newState)
        {
            const char *stateStr{"Unknown"};
            switch (newState)
            {
            case ara::ucm::UpdateSessionState::kIdle:              stateStr = "Idle";              break;
            case ara::ucm::UpdateSessionState::kPrepared:          stateStr = "Prepared";          break;
            case ara::ucm::UpdateSessionState::kTransferring:      stateStr = "Transferring";      break;
            case ara::ucm::UpdateSessionState::kPackageStaged:     stateStr = "PackageStaged";     break;
            case ara::ucm::UpdateSessionState::kPackageVerified:   stateStr = "PackageVerified";   break;
            case ara::ucm::UpdateSessionState::kActivating:        stateStr = "Activating";        break;
            case ara::ucm::UpdateSessionState::kActivated:         stateStr = "Activated";         break;
            case ara::ucm::UpdateSessionState::kVerificationFailed:stateStr = "VerificationFailed";break;
            case ara::ucm::UpdateSessionState::kRolledBack:        stateStr = "RolledBack";        break;
            case ara::ucm::UpdateSessionState::kCancelled:         stateStr = "Cancelled";         break;
            default:                                                                                  break;
            }
            Log(ara::log::LogLevel::kInfo,
                std::string("UCM state -> ") + stateStr);
        });

    (void)ucmMgr.SetProgressHandler(
        [&logging, &logger](std::uint8_t pct)
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "UCM progress: " << static_cast<unsigned>(pct) << "%";
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        });

    // Compute SHA-256 of a data buffer via OpenSSL.
    auto ComputeSha256 = [](const std::vector<std::uint8_t> &data)
        -> std::vector<std::uint8_t>
    {
        std::vector<std::uint8_t> digest(SHA256_DIGEST_LENGTH);
        SHA256(data.data(), data.size(), digest.data());
        return digest;
    };

    // Run initial software-update simulation (prepare -> stage -> verify -> activate).
    auto RunUcmSimulation = [&]()
    {
        Log(ara::log::LogLevel::kInfo, "UCM: Starting update simulation...");

        // Transition SM to Update state.
        (void)machineState.SetMachineState(ara::sm::MachineState::kUpdate);

        auto prepResult{ucmMgr.PrepareUpdate("raspi_ecu_session_001")};
        if (!prepResult.HasValue())
        {
            Log(ara::log::LogLevel::kWarn,
                "UCM: PrepareUpdate failed: " + prepResult.Error().Message());
            (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
            return;
        }

        // Simulate a 64-byte firmware payload.
        const std::vector<std::uint8_t> fwPayload(64U, 0xA5U);
        const auto fwDigest{ComputeSha256(fwPayload)};

        ara::ucm::SoftwarePackageMetadata meta;
        meta.PackageName   = "raspi_ecu_fw";
        meta.TargetCluster = "Cluster_Main";
        meta.Version       = "1.0.1";

        auto stageResult{ucmMgr.StageSoftwarePackage(meta, fwPayload, fwDigest)};
        if (!stageResult.HasValue())
        {
            Log(ara::log::LogLevel::kWarn,
                "UCM: StageSoftwarePackage failed: " + stageResult.Error().Message());
            (void)ucmMgr.CancelUpdateSession();
            (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
            return;
        }

        auto verifyResult{ucmMgr.VerifyStagedSoftwarePackage()};
        if (!verifyResult.HasValue())
        {
            Log(ara::log::LogLevel::kWarn,
                "UCM: VerifyStagedSoftwarePackage failed: " +
                verifyResult.Error().Message());
            (void)ucmMgr.CancelUpdateSession();
            (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
            return;
        }

        auto activateResult{ucmMgr.ActivateSoftwarePackage()};
        if (activateResult.HasValue())
        {
            Log(ara::log::LogLevel::kInfo, "UCM: Update simulation complete (Activated)");
        }
        else
        {
            Log(ara::log::LogLevel::kWarn,
                "UCM: ActivateSoftwarePackage failed: " + activateResult.Error().Message());
        }

        // Return SM to normal operation.
        (void)machineState.SetMachineState(ara::sm::MachineState::kRunning);
    };

    RunUcmSimulation();

    // ------------------------------------------------------------------
    // 12. Main simulation loop
    // ------------------------------------------------------------------
    Log(ara::log::LogLevel::kInfo, "=== Main loop started ===");

    std::uint32_t loopCycle{0U};
    auto nextTick{std::chrono::steady_clock::now()};

    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // Respect --max-cycles if specified.
        if (cfg.MaxCycles > 0U && loopCycle >= cfg.MaxCycles)
        {
            Log(ara::log::LogLevel::kInfo, "Max cycles reached, stopping.");
            break;
        }

        // --- 12-a) Application liveness ---
        appClient.ReportApplicationHealth();

        // --- 12-b) PHM health ---
        (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kOk);

        // --- 12-c) ara::tsync — feed a mock NTP reference sample every 5 cycles ---
        if ((loopCycle % 5U) == 0U)
        {
            (void)timeSyncClient.UpdateReferenceTime(
                std::chrono::system_clock::now(),
                std::chrono::steady_clock::now());
            timeSyncClient.CheckSyncTimeout();
        }

        // --- 12-d) ara::nm — NM tick and periodic NM message indication ---
        const std::uint64_t nowMs{NowEpochMs()};
        nmMgr.Tick(nowMs);

        if ((loopCycle % 6U) == 0U)
        {
            // Simulate receiving an NM PDU from a peer on the CAN bus.
            (void)nmMgr.NmMessageIndication(kCanChannel);
        }

        // --- 12-e) ara::diag — simulate one UDS request per cycle ---
        {
            // Rotate through a few services for variety.
            static const std::uint8_t kServices[]{
                0x22U, 0x19U, 0x3EU, 0x10U, 0x14U};
            const std::uint8_t svc{
                kServices[loopCycle % (sizeof(kServices) / sizeof(kServices[0]))]};

            const std::vector<std::uint8_t> payload{svc, 0x00U, 0x01U};
            auto reqResult{diagMgr.SubmitRequest(svc, 0U, payload)};
            if (reqResult.HasValue())
            {
                // Complete the request immediately (positive response).
                (void)diagMgr.CompleteRequest(reqResult.Value());
                ++diagRequestCount;
            }
            diagMgr.CheckTimingConstraints();
        }

        // --- 12-f) ara::crypto — encrypt a vehicle telemetry payload every 8 cycles ---
        if (cfg.VerboseCrypto && (loopCycle % 8U) == 0U && payloadKeySlot > 0U)
        {
            // Simulate an 80-byte vehicle telemetry packet.
            std::vector<std::uint8_t> telemetry(80U, 0U);
            for (std::size_t i = 0U; i < telemetry.size(); ++i)
            {
                telemetry[i] = static_cast<std::uint8_t>(i ^ loopCycle);
            }
            auto encResult{hsm.Encrypt(payloadKeySlot, telemetry)};
            if (encResult.Success)
            {
                // Authenticate with HMAC.
                if (hmacKeySlot > 0U)
                {
                    (void)hsm.Sign(hmacKeySlot, encResult.OutputData);
                }
                auto stream{logger.WithLevel(ara::log::LogLevel::kVerbose)};
                stream << "Crypto: encrypted " << static_cast<std::uint32_t>(telemetry.size())
                       << "B -> " << static_cast<std::uint32_t>(encResult.OutputData.size()) << "B";
                logging->Log(logger, ara::log::LogLevel::kVerbose, stream);
            }
        }

        // --- 12-g) ara::ucm — re-run UCM simulation periodically ---
        if (cfg.UcmEvery > 0U && loopCycle > 0U && (loopCycle % cfg.UcmEvery) == 0U)
        {
            // Reset UCM manager and repeat a staged update cycle.
            auto cancelResult{ucmMgr.CancelUpdateSession()};
            if (cancelResult.HasValue())
            {
                RunUcmSimulation();
            }
        }

        // --- 12-h) Persist counters every 20 cycles ---
        ++cycleCount;
        if ((loopCycle % 20U) == 0U)
        {
            SaveCounter("raspi_ecu.cycle_count", cycleCount);
            SaveCounter("raspi_ecu.diag_requests", diagRequestCount);
            if (storage) { (void)storage->SyncToStorage(); }
        }

        // --- 12-i) Status log ---
        if (cfg.LogEvery > 0U && (loopCycle % cfg.LogEvery) == 0U)
        {
            const auto smState{machineState.GetMachineState()};
            const auto tsState{timeSyncClient.GetState()};
            const auto nmStatus{nmMgr.GetChannelStatus(kCanChannel)};

            auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
            stream << "[cycle=" << loopCycle
                   << "] sm=" << (smState.HasValue() ? MachineStateStr(smState.Value()) : "?")
                   << " tsync=" << SyncStateStr(tsState)
                   << " nm=" << (nmStatus.HasValue() ? NmStateStr(nmStatus.Value().State) : "?")
                   << " diag_total=" << static_cast<std::uint32_t>(diagRequestCount)
                   << " health_reports=" << static_cast<std::uint32_t>(appClient.GetHealthReportCount())
                   << " crypto_ok=" << (cryptoSelfTestOk && hmacSelfTestOk ? "Y" : "N");
            logging->Log(logger, ara::log::LogLevel::kInfo, stream);
        }

        ++loopCycle;

        // --- 12-j) Rate limiting ---
        nextTick += std::chrono::milliseconds{cfg.PeriodMs};
        const auto now{std::chrono::steady_clock::now()};
        if (nextTick > now)
        {
            std::this_thread::sleep_until(nextTick);
        }
        else
        {
            // We're behind; catch up without sleeping.
            nextTick = now + std::chrono::milliseconds{cfg.PeriodMs};
        }
    }

    // ------------------------------------------------------------------
    // 13. Graceful shutdown
    // ------------------------------------------------------------------
    Log(ara::log::LogLevel::kInfo, "=== Graceful shutdown ===");

    // Persist final counters.
    SaveCounter("raspi_ecu.cycle_count", cycleCount);
    SaveCounter("raspi_ecu.diag_requests", diagRequestCount);
    if (storage) { (void)storage->SyncToStorage(); }

    // Release the CAN network (allows the channel to sleep).
    (void)nmMgr.NetworkRelease(kCanChannel);
    nmMgr.Tick(NowEpochMs());

    // Finalise HSM session.
    hsm.Finalize();

    // Transition machine to shutdown state.
    (void)machineState.SetMachineState(ara::sm::MachineState::kShutdown);

    // Report PHM deactivation.
    (void)healthChannel.ReportHealthStatus(ara::phm::HealthStatus::kDeactivated);

    // Print final statistics.
    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Final stats: cycles=" << loopCycle
               << " boot=" << static_cast<std::uint32_t>(bootCount)
               << " diag_requests=" << static_cast<std::uint32_t>(diagRequestCount)
               << " health_reports=" << static_cast<std::uint32_t>(appClient.GetHealthReportCount());
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    Log(ara::log::LogLevel::kInfo, "=== Raspberry Pi Mock ECU stopped ===");

    (void)ara::core::Deinitialize();
    return 0;
}
