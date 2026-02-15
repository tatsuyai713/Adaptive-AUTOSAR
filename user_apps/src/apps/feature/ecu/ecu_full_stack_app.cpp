#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <ara/com/zerocopy/zero_copy_binding.h>
#include <ara/core/initialization.h>
#include <ara/core/instance_specifier.h>
#include <ara/exec/signal_handler.h>
#include <ara/log/logging_framework.h>

#include "../../../features/communication/pubsub/pubsub_autosar_portable_api.h"
#include "../../../features/communication/pubsub/pubsub_common.h"
#include "../../../features/communication/can/vehicle_status_can_decoder.h"
#include "../../../features/ecu/ecu_sample_common.h"

namespace
{
    // Runtime options for the full ECU reference template.
    struct RuntimeConfig
    {
        bool EnableCanInput{true};
        bool EnableSomeIpInput{true};
        bool RequireSomeIpInput{false};
        bool RequireBothSources{false};
        bool EnableZeroCopyLocalPublish{false};

        std::string CanBackend{"socketcan"};
        std::string CanInterface{"can0"};
        std::uint32_t ReceiveTimeoutMs{20U};
        std::uint32_t PowertrainCanId{0x100U};
        std::uint32_t ChassisCanId{0x101U};

        std::uint32_t DdsDomainId{sample::ara_com_pubsub::cDdsDomainId};
        std::string DdsTopicName{sample::ara_com_pubsub::cDdsTopicName};

        std::uint32_t PublishPeriodMs{50U};
        std::uint32_t SourceStaleMs{500U};
        std::uint32_t ServiceWaitMs{5000U};
        std::uint64_t StorageSyncEvery{100U};
        std::uint64_t LogEvery{20U};
        std::string ZeroCopyRuntimeName{"autosar_user_tpl_ecu_full_stack"};

        std::string ProviderInstanceSpecifier{
            "AdaptiveAutosar/UserApps/Templates/EcuFullStack/Provider"};
        std::string HealthInstanceSpecifier{
            "AdaptiveAutosar/UserApps/Templates/EcuFullStack/Health"};
        std::string StorageInstanceSpecifier{
            "AdaptiveAutosar/UserApps/Templates/EcuFullStack/Storage"};
    };

    // Shared input snapshot updated by CAN and SOME/IP callbacks.
    struct InputState
    {
        std::mutex Mutex;
        bool HasCan{false};
        bool HasSomeIp{false};
        sample::ara_com_pubsub::VehicleStatusFrame CanFrame{};
        sample::ara_com_pubsub::VehicleStatusFrame SomeIpFrame{};
        std::chrono::steady_clock::time_point CanTimestamp{};
        std::chrono::steady_clock::time_point SomeIpTimestamp{};
        std::uint32_t NextSequence{1U};
    };

    // Metadata for selecting how one output frame was built.
    enum class OutputMode : std::uint8_t
    {
        kNone = 0U,
        kCanOnly = 1U,
        kSomeIpOnly = 2U,
        kFusedCanAndSomeIp = 3U
    };

    // Convert enum to text for logs/persistency.
    const char *ToString(OutputMode mode) noexcept
    {
        switch (mode)
        {
        case OutputMode::kCanOnly:
            return "can_only";
        case OutputMode::kSomeIpOnly:
            return "someip_only";
        case OutputMode::kFusedCanAndSomeIp:
            return "fused";
        case OutputMode::kNone:
        default:
            return "none";
        }
    }

    // Parse all command-line options used by this template.
    RuntimeConfig ParseRuntimeConfig(int argc, char *argv[])
    {
        RuntimeConfig config;
        std::string value;

        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--enable-can", value))
        {
            config.EnableCanInput = sample::ecu_showcase::ParseBool(value, config.EnableCanInput);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--enable-someip", value))
        {
            config.EnableSomeIpInput = sample::ecu_showcase::ParseBool(value, config.EnableSomeIpInput);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--require-someip", value))
        {
            config.RequireSomeIpInput = sample::ecu_showcase::ParseBool(value, config.RequireSomeIpInput);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--require-both-sources", value))
        {
            config.RequireBothSources = sample::ecu_showcase::ParseBool(value, config.RequireBothSources);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--enable-zerocopy-local", value))
        {
            config.EnableZeroCopyLocalPublish =
                sample::ecu_showcase::ParseBool(value, config.EnableZeroCopyLocalPublish);
        }

        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--can-backend", value) && !value.empty())
        {
            config.CanBackend = value;
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--ifname", value) && !value.empty())
        {
            config.CanInterface = value;
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--recv-timeout-ms", value))
        {
            config.ReceiveTimeoutMs = sample::ecu_showcase::ParseU32(value, config.ReceiveTimeoutMs);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--powertrain-can-id", value))
        {
            config.PowertrainCanId = sample::ecu_showcase::ParseU32(value, config.PowertrainCanId);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--chassis-can-id", value))
        {
            config.ChassisCanId = sample::ecu_showcase::ParseU32(value, config.ChassisCanId);
        }

        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--dds-domain-id", value))
        {
            config.DdsDomainId = sample::ecu_showcase::ParseU32(value, config.DdsDomainId);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--dds-topic", value) && !value.empty())
        {
            config.DdsTopicName = value;
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--publish-period-ms", value))
        {
            config.PublishPeriodMs = sample::ecu_showcase::ParseU32(value, config.PublishPeriodMs);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--source-stale-ms", value))
        {
            config.SourceStaleMs = sample::ecu_showcase::ParseU32(value, config.SourceStaleMs);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--service-wait-ms", value))
        {
            config.ServiceWaitMs = sample::ecu_showcase::ParseU32(value, config.ServiceWaitMs);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--storage-sync-every", value))
        {
            config.StorageSyncEvery = sample::ecu_showcase::ParseU64(value, config.StorageSyncEvery);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--log-every", value))
        {
            config.LogEvery = sample::ecu_showcase::ParseU64(value, config.LogEvery);
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--zerocopy-runtime-name", value) &&
            !value.empty())
        {
            config.ZeroCopyRuntimeName = value;
        }

        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--provider-instance", value) &&
            !value.empty())
        {
            config.ProviderInstanceSpecifier = value;
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--health-instance", value) &&
            !value.empty())
        {
            config.HealthInstanceSpecifier = value;
        }
        if (sample::ecu_showcase::TryReadArgument(argc, argv, "--storage-instance", value) &&
            !value.empty())
        {
            config.StorageInstanceSpecifier = value;
        }

        return config;
    }

    // Convert one path string into AUTOSAR InstanceSpecifier or throw.
    ara::core::InstanceSpecifier CreateSpecifierOrThrow(const std::string &path)
    {
        auto specifierResult{ara::core::InstanceSpecifier::Create(path)};
        if (!specifierResult.HasValue())
        {
            throw std::runtime_error(specifierResult.Error().Message());
        }

        return specifierResult.Value();
    }

    // Logging helper to keep the business code readable.
    void LogText(
        ara::log::LoggingFramework &logging,
        const ara::log::Logger &logger,
        ara::log::LogLevel level,
        const std::string &message)
    {
        auto stream{logger.WithLevel(level)};
        stream << message;
        logging.Log(logger, level, stream);
    }

    // Build one output frame from latest fresh inputs.
    bool TryBuildOutputFrame(
        InputState &state,
        bool canEnabled,
        bool someIpEnabled,
        bool requireBothSources,
        std::chrono::milliseconds sourceStaleThreshold,
        sample::ara_com_pubsub::VehicleStatusFrame &outputFrame,
        OutputMode &outputMode)
    {
        const auto now{std::chrono::steady_clock::now()};

        std::lock_guard<std::mutex> lock(state.Mutex);

        const bool canFresh{
            canEnabled &&
            state.HasCan &&
            (now - state.CanTimestamp) <= sourceStaleThreshold};
        const bool someIpFresh{
            someIpEnabled &&
            state.HasSomeIp &&
            (now - state.SomeIpTimestamp) <= sourceStaleThreshold};

        if (requireBothSources && !(canFresh && someIpFresh))
        {
            outputMode = OutputMode::kNone;
            return false;
        }

        if (!canFresh && !someIpFresh)
        {
            outputMode = OutputMode::kNone;
            return false;
        }

        if (canFresh && someIpFresh)
        {
            // Fuse the two sources to produce one robust output snapshot.
            outputMode = OutputMode::kFusedCanAndSomeIp;
            outputFrame = state.CanFrame;
            outputFrame.SpeedCentiKph =
                static_cast<std::uint32_t>(
                    (state.CanFrame.SpeedCentiKph + state.SomeIpFrame.SpeedCentiKph) / 2U);
            outputFrame.EngineRpm =
                static_cast<std::uint32_t>(
                    (state.CanFrame.EngineRpm + state.SomeIpFrame.EngineRpm) / 2U);
            outputFrame.SteeringAngleCentiDeg = state.SomeIpFrame.SteeringAngleCentiDeg;
            outputFrame.Gear = state.SomeIpFrame.Gear;
            outputFrame.StatusFlags =
                static_cast<std::uint8_t>(state.CanFrame.StatusFlags | state.SomeIpFrame.StatusFlags);
        }
        else if (canFresh)
        {
            outputMode = OutputMode::kCanOnly;
            outputFrame = state.CanFrame;
        }
        else
        {
            outputMode = OutputMode::kSomeIpOnly;
            outputFrame = state.SomeIpFrame;
        }

        // Assign ECU-owned sequence counter after fusion decision.
        outputFrame.SequenceCounter = state.NextSequence++;
        return true;
    }
}

int main(int argc, char *argv[])
{
#if !defined(ARA_COM_USE_CYCLONEDDS) || (ARA_COM_USE_CYCLONEDDS != 1)
    (void)argc;
    (void)argv;
    std::cout << "[TplEcuFullStack] DDS backend is disabled. "
                 "Rebuild runtime with ARA_COM_USE_CYCLONEDDS=ON."
              << std::endl;
    return 0;
#else
    const RuntimeConfig config{ParseRuntimeConfig(argc, argv)};

    // 1) Initialize runtime and process signal handling.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TplEcuFullStack] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }
    ara::exec::SignalHandler::Register();

    // 2) Setup logger.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UFST",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User template: ECU full stack (CAN/SOMEIP->DDS)")}};
    const auto &logger{
        logging->CreateLogger(
            "ECUF",
            "ECU full stack template",
            ara::log::LogLevel::kInfo)};

    // 3) Setup PHM and PER helpers.
    sample::ecu_showcase::HealthReporter health{config.HealthInstanceSpecifier};
    sample::ecu_showcase::PersistentCounterStore storage{config.StorageInstanceSpecifier};
    health.ReportOk();

    std::uint64_t canRxTotal{storage.Load("ecu_full.can_rx_total", 0U)};
    std::uint64_t someIpRxTotal{storage.Load("ecu_full.someip_rx_total", 0U)};
    std::uint64_t ddsTxTotal{storage.Load("ecu_full.dds_tx_total", 0U)};
    std::uint64_t zeroCopyTxTotal{storage.Load("ecu_full.zerocopy_tx_total", 0U)};
    std::uint64_t publishCountSinceSync{0U};

    // 4) Build DDS output provider through portable ara::com API.
    sample::ara_com_pubsub::portable::VehicleStatusProvider ddsProvider{
        CreateSpecifierOrThrow(config.ProviderInstanceSpecifier),
        sample::ecu_showcase::BuildDdsProfile(config.DdsDomainId, config.DdsTopicName)};

    auto offerServiceResult{ddsProvider.OfferService()};
    if (!offerServiceResult.HasValue())
    {
        LogText(
            *logging,
            logger,
            ara::log::LogLevel::kError,
            "OfferService failed: " + offerServiceResult.Error().Message());
        health.ReportFailed();
        (void)ara::core::Deinitialize();
        return 1;
    }

    auto offerEventResult{ddsProvider.OfferEvent()};
    if (!offerEventResult.HasValue())
    {
        LogText(
            *logging,
            logger,
            ara::log::LogLevel::kError,
            "OfferEvent failed: " + offerEventResult.Error().Message());
        ddsProvider.StopOfferService();
        health.ReportFailed();
        (void)ara::core::Deinitialize();
        return 1;
    }

    // 5) Optionally configure local zero-copy publication for in-node consumers.
    std::unique_ptr<ara::com::zerocopy::ZeroCopyPublisher> zeroCopyPublisher;
    if (config.EnableZeroCopyLocalPublish)
    {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
        auto createPublisherResult{
            ddsProvider.CreateZeroCopyPublisher(config.ZeroCopyRuntimeName, 8U)};
        if (createPublisherResult.HasValue())
        {
            zeroCopyPublisher.reset(
                new ara::com::zerocopy::ZeroCopyPublisher{
                    std::move(createPublisherResult).Value()});
            LogText(
                *logging,
                logger,
                ara::log::LogLevel::kInfo,
                "ZeroCopy local publisher enabled.");
        }
        else
        {
            LogText(
                *logging,
                logger,
                ara::log::LogLevel::kWarn,
                "CreateZeroCopyPublisher failed: " + createPublisherResult.Error().Message());
        }
#else
        LogText(
            *logging,
            logger,
            ara::log::LogLevel::kWarn,
            "ZeroCopy requested but ARA_COM_USE_ICEORYX is disabled.");
#endif
    }

    // 6) Initialize CAN input path.
    std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver> canReceiver;
    sample::ara_com_socketcan_gateway::VehicleStatusCanDecoder canDecoder{
        sample::ara_com_socketcan_gateway::VehicleStatusCanDecoder::Config{
            config.PowertrainCanId,
            config.ChassisCanId,
            false}};

    if (config.EnableCanInput)
    {
        canReceiver = sample::ecu_showcase::CreateCanReceiver(
            config.CanBackend,
            config.CanInterface,
            config.ReceiveTimeoutMs);
        if (!canReceiver)
        {
            LogText(
                *logging,
                logger,
                ara::log::LogLevel::kError,
                "Unsupported --can-backend value: " + config.CanBackend);
            ddsProvider.StopOfferEvent();
            ddsProvider.StopOfferService();
            health.ReportFailed();
            (void)ara::core::Deinitialize();
            return 1;
        }

        auto openResult{canReceiver->Open()};
        if (!openResult.HasValue())
        {
            LogText(
                *logging,
                logger,
                ara::log::LogLevel::kError,
                "CAN receiver open failed: " + openResult.Error().Message());
            ddsProvider.StopOfferEvent();
            ddsProvider.StopOfferService();
            health.ReportFailed();
            (void)ara::core::Deinitialize();
            return 1;
        }
    }

    // 7) Initialize SOME/IP input path.
    std::mutex serviceHandleMutex;
    std::vector<sample::ara_com_pubsub::portable::VehicleStatusServiceHandle> serviceHandles;
    bool findServiceStarted{false};
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
    if (config.EnableSomeIpInput)
    {
        auto findServiceResult{
            sample::ara_com_pubsub::portable::VehicleStatusConsumer::StartFindService(
                [&serviceHandleMutex, &serviceHandles](
                    std::vector<sample::ara_com_pubsub::portable::VehicleStatusServiceHandle> handles)
                {
                    std::lock_guard<std::mutex> lock(serviceHandleMutex);
                    serviceHandles = std::move(handles);
                },
                sample::ecu_showcase::BuildSomeIpProfile())};

        if (!findServiceResult.HasValue())
        {
            LogText(
                *logging,
                logger,
                ara::log::LogLevel::kWarn,
                "StartFindService failed: " + findServiceResult.Error().Message());
        }
        else
        {
            findServiceStarted = true;
        }
    }
#else
    if (config.EnableSomeIpInput)
    {
        LogText(
            *logging,
            logger,
            ara::log::LogLevel::kWarn,
            "SOME/IP input requested but ARA_COM_USE_VSOMEIP is disabled.");
    }
#endif

    InputState inputState;
    std::unique_ptr<sample::ara_com_pubsub::portable::VehicleStatusConsumer> someIpConsumer;
    std::atomic_bool someIpSubscribed{false};

    // Optionally wait for SOME/IP service if this ECU role requires it.
    if (config.RequireSomeIpInput)
    {
        const auto deadline{
            std::chrono::steady_clock::now() + std::chrono::milliseconds{config.ServiceWaitMs}};
        while (std::chrono::steady_clock::now() < deadline &&
               !ara::exec::SignalHandler::IsTerminationRequested())
        {
            bool hasHandle{false};
            {
                std::lock_guard<std::mutex> lock(serviceHandleMutex);
                hasHandle = !serviceHandles.empty();
            }
            if (hasHandle)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50U});
        }
    }

    // 8) Main ECU processing loop: ingest CAN/SOME-IP, publish DDS.
    LogText(
        *logging,
        logger,
        ara::log::LogLevel::kInfo,
        "ECU full stack template started.");

    auto nextPublishTime{
        std::chrono::steady_clock::now() + std::chrono::milliseconds{config.PublishPeriodMs}};
    while (!ara::exec::SignalHandler::IsTerminationRequested())
    {
        // 8-a) Poll one CAN frame and update shared input state.
        if (canReceiver)
        {
            sample::ara_com_socketcan_gateway::CanFrame canFrame{};
            auto receiveResult{
                canReceiver->Receive(
                    canFrame,
                    std::chrono::milliseconds{config.ReceiveTimeoutMs})};
            if (receiveResult.HasValue() && receiveResult.Value())
            {
                sample::vehicle_status::VehicleStatusFrame decoded{};
                if (canDecoder.TryDecode(canFrame, decoded))
                {
                    {
                        std::lock_guard<std::mutex> lock(inputState.Mutex);
                        inputState.HasCan = true;
                        inputState.CanFrame = sample::ecu_showcase::ToPortableFrame(decoded);
                        inputState.CanTimestamp = std::chrono::steady_clock::now();
                    }
                    ++canRxTotal;
                }
            }
        }

        // 8-b) Create SOME/IP consumer once service is discovered.
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
        if (config.EnableSomeIpInput && findServiceStarted && !someIpConsumer)
        {
            sample::ara_com_pubsub::portable::VehicleStatusServiceHandle selectedHandle{
                sample::ara_com_pubsub::cServiceId,
                sample::ara_com_pubsub::cInstanceId,
                sample::ara_com_pubsub::portable::EventBackend::kSomeIp};
            bool hasHandle{false};

            {
                std::lock_guard<std::mutex> lock(serviceHandleMutex);
                if (!serviceHandles.empty())
                {
                    selectedHandle = serviceHandles.front();
                    hasHandle = true;
                }
            }

            if (hasHandle)
            {
                someIpConsumer.reset(
                    new sample::ara_com_pubsub::portable::VehicleStatusConsumer{
                        selectedHandle,
                        sample::ecu_showcase::BuildSomeIpProfile()});

                auto subscribeResult{
                    someIpConsumer->Subscribe(
                        [&inputState, &someIpRxTotal](const std::vector<std::uint8_t> &payload)
                        {
                            sample::ara_com_pubsub::VehicleStatusFrame frame{};
                            if (!sample::ara_com_pubsub::DeserializeFrame(payload, frame))
                            {
                                return;
                            }

                            std::lock_guard<std::mutex> lock(inputState.Mutex);
                            inputState.HasSomeIp = true;
                            inputState.SomeIpFrame = frame;
                            inputState.SomeIpTimestamp = std::chrono::steady_clock::now();
                            ++someIpRxTotal;
                        },
                        sample::ara_com_pubsub::cMajorVersion)};

                if (subscribeResult.HasValue())
                {
                    someIpSubscribed.store(true);
                    LogText(
                        *logging,
                        logger,
                        ara::log::LogLevel::kInfo,
                        "SOME/IP subscription established.");
                }
                else
                {
                    LogText(
                        *logging,
                        logger,
                        ara::log::LogLevel::kWarn,
                        "SOME/IP subscribe failed: " + subscribeResult.Error().Message());
                    someIpConsumer.reset();
                }
            }
        }
#endif

        // 8-c) Publish one fused/selected frame at configured period.
        const auto now{std::chrono::steady_clock::now()};
        if (now >= nextPublishTime)
        {
            sample::ara_com_pubsub::VehicleStatusFrame output{};
            OutputMode mode{OutputMode::kNone};

            const bool built{
                TryBuildOutputFrame(
                    inputState,
                    config.EnableCanInput,
                    config.EnableSomeIpInput,
                    config.RequireBothSources,
                    std::chrono::milliseconds{config.SourceStaleMs},
                    output,
                    mode)};

            if (built)
            {
                const auto payload{sample::ara_com_pubsub::SerializeFrame(output)};
                auto notifyResult{ddsProvider.NotifyEvent(payload, true)};
                if (notifyResult.HasValue())
                {
                    ++ddsTxTotal;
                    ++publishCountSinceSync;
                }
                else
                {
                    LogText(
                        *logging,
                        logger,
                        ara::log::LogLevel::kWarn,
                        "DDS NotifyEvent failed: " + notifyResult.Error().Message());
                }

                // Optional local zero-copy publish of the same payload.
                if (zeroCopyPublisher)
                {
                    ara::com::zerocopy::LoanedSample sample;
                    auto loanResult{
                        zeroCopyPublisher->Loan(payload.size(), sample, alignof(std::uint8_t))};
                    if (loanResult.HasValue())
                    {
                        std::memcpy(sample.Data(), payload.data(), payload.size());
                        auto publishResult{zeroCopyPublisher->Publish(std::move(sample))};
                        if (publishResult.HasValue())
                        {
                            ++zeroCopyTxTotal;
                        }
                    }
                }

                if ((config.LogEvery > 0U) && ((ddsTxTotal % config.LogEvery) == 0U))
                {
                    auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
                    stream << "Published seq=" << output.SequenceCounter
                           << " mode=" << ToString(mode)
                           << " can_rx_total=" << static_cast<std::uint32_t>(canRxTotal)
                           << " someip_rx_total=" << static_cast<std::uint32_t>(someIpRxTotal)
                           << " dds_tx_total=" << static_cast<std::uint32_t>(ddsTxTotal)
                           << " zerocopy_tx_total=" << static_cast<std::uint32_t>(zeroCopyTxTotal);
                    logging->Log(logger, ara::log::LogLevel::kInfo, stream);
                }

                if ((config.StorageSyncEvery > 0U) &&
                    (publishCountSinceSync >= config.StorageSyncEvery))
                {
                    storage.Save("ecu_full.can_rx_total", canRxTotal);
                    storage.Save("ecu_full.someip_rx_total", someIpRxTotal);
                    storage.Save("ecu_full.dds_tx_total", ddsTxTotal);
                    storage.Save("ecu_full.zerocopy_tx_total", zeroCopyTxTotal);
                    storage.Sync();
                    publishCountSinceSync = 0U;
                }
            }

            nextPublishTime += std::chrono::milliseconds{config.PublishPeriodMs};
            if (nextPublishTime < std::chrono::steady_clock::now())
            {
                nextPublishTime =
                    std::chrono::steady_clock::now() + std::chrono::milliseconds{config.PublishPeriodMs};
            }
        }

        // Keep loop responsive without busy spinning.
        std::this_thread::sleep_for(std::chrono::milliseconds{2U});
    }

    // 9) Shutdown: unsubscribe inputs and stop outputs.
#if defined(ARA_COM_USE_VSOMEIP) && (ARA_COM_USE_VSOMEIP == 1)
    if (someIpConsumer && someIpSubscribed.load())
    {
        someIpConsumer->Unsubscribe();
    }
    if (findServiceStarted)
    {
        sample::ara_com_pubsub::portable::VehicleStatusConsumer::StopFindService();
    }
#endif

    if (canReceiver)
    {
        canReceiver->Close();
    }

    ddsProvider.StopOfferEvent();
    ddsProvider.StopOfferService();

    storage.Save("ecu_full.can_rx_total", canRxTotal);
    storage.Save("ecu_full.someip_rx_total", someIpRxTotal);
    storage.Save("ecu_full.dds_tx_total", ddsTxTotal);
    storage.Save("ecu_full.zerocopy_tx_total", zeroCopyTxTotal);
    storage.Sync();

    health.ReportDeactivated();
    (void)ara::core::Deinitialize();
    return 0;
#endif
}
