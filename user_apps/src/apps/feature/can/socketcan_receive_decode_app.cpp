#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

#include "../../../features/communication/can/can_frame_receiver.h"
#include "../../../features/communication/can/mock_can_receiver.h"
#include "../../../features/communication/can/socketcan_receiver.h"
#include "../../../features/communication/can/vehicle_status_can_decoder.h"

namespace
{
    // Global flag toggled by SIGINT/SIGTERM for graceful stop.
    std::atomic_bool gRunning{true};

    // Handle Ctrl+C / SIGTERM by requesting loop shutdown.
    void HandleSignal(int)
    {
        gRunning.store(false);
    }

    // Register basic process signal handlers.
    void RegisterSignalHandlers()
    {
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
    }

    // Runtime options for this template.
    struct RuntimeConfig
    {
        std::string CanBackend{"socketcan"};
        std::string CanInterface{"can0"};
        std::uint32_t ReceiveTimeoutMs{20U};
        std::uint32_t PowertrainCanId{0x100U};
        std::uint32_t ChassisCanId{0x101U};
        bool RequireBothFramesBeforeDecode{true};
    };

    // Parse --key=value style argument.
    bool TryReadArgument(
        int argc,
        char *argv[],
        const std::string &name,
        std::string &value)
    {
        const std::string prefix{name + "="};
        for (int index{1}; index < argc; ++index)
        {
            const std::string argument{argv[index]};
            if (argument.find(prefix) == 0U)
            {
                value = argument.substr(prefix.size());
                return true;
            }
        }

        return false;
    }

    // Parse unsigned integer from text with fallback.
    std::uint32_t ParseU32(const std::string &text, std::uint32_t fallback) noexcept
    {
        if (text.empty())
        {
            return fallback;
        }

        try
        {
            std::size_t parsedLength{0U};
            const auto parsed{std::stoul(text, &parsedLength, 0)};
            if (parsedLength != text.size())
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

    // Parse boolean option with common textual values.
    bool ParseBool(const std::string &text, bool fallback) noexcept
    {
        if (text == "1" || text == "true" || text == "TRUE" || text == "on" || text == "ON")
        {
            return true;
        }
        if (text == "0" || text == "false" || text == "FALSE" || text == "off" || text == "OFF")
        {
            return false;
        }
        return fallback;
    }

    // Create concrete receiver backend from runtime config.
    std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>
    CreateCanReceiver(const RuntimeConfig &config)
    {
        if (config.CanBackend == "socketcan")
        {
            return std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>{
                new sample::ara_com_socketcan_gateway::SocketCanReceiver{config.CanInterface}};
        }
        if (config.CanBackend == "mock")
        {
            return std::unique_ptr<sample::ara_com_socketcan_gateway::CanFrameReceiver>{
                new sample::ara_com_socketcan_gateway::MockCanReceiver{
                    std::chrono::milliseconds{config.ReceiveTimeoutMs}}};
        }

        return nullptr;
    }

    // Parse command-line options in --key=value form.
    RuntimeConfig ParseRuntimeConfig(int argc, char *argv[])
    {
        RuntimeConfig config;
        std::string value;

        if (TryReadArgument(
                argc,
                argv,
                "--can-backend",
                value) &&
            !value.empty())
        {
            config.CanBackend = value;
        }

        if (TryReadArgument(
                argc,
                argv,
                "--ifname",
                value) &&
            !value.empty())
        {
            config.CanInterface = value;
        }

        if (TryReadArgument(
                argc,
                argv,
                "--recv-timeout-ms",
                value))
        {
            config.ReceiveTimeoutMs =
                ParseU32(
                    value,
                    config.ReceiveTimeoutMs);
        }

        if (TryReadArgument(
                argc,
                argv,
                "--powertrain-can-id",
                value))
        {
            config.PowertrainCanId =
                ParseU32(
                    value,
                    config.PowertrainCanId);
        }

        if (TryReadArgument(
                argc,
                argv,
                "--chassis-can-id",
                value))
        {
            config.ChassisCanId =
                ParseU32(
                    value,
                    config.ChassisCanId);
        }

        if (TryReadArgument(
                argc,
                argv,
                "--require-both-frames",
                value))
        {
            config.RequireBothFramesBeforeDecode =
                ParseBool(
                    value,
                    config.RequireBothFramesBeforeDecode);
        }

        return config;
    }
}

int main(int argc, char *argv[])
{
    RegisterSignalHandlers();
    const RuntimeConfig config{ParseRuntimeConfig(argc, argv)};

    // 1) Initialize runtime before all ara::* calls.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        std::cerr << "[TplSocketCanReceiveDecode] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Setup logger used by this template.
    auto logging{std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UTCS",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "User template: SocketCAN receive + decode")}};

    const auto &logger{
        logging->CreateLogger(
            "CANR",
            "SocketCAN receive/decode template",
            ara::log::LogLevel::kInfo)};

    // 3) Create CAN backend adapter (socketcan or mock).
    auto receiver{CreateCanReceiver(config)};
    if (!receiver)
    {
        std::cerr << "[TplSocketCanReceiveDecode] Unsupported backend: "
                  << config.CanBackend << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    auto openResult{receiver->Open()};
    if (!openResult.HasValue())
    {
        std::cerr << "[TplSocketCanReceiveDecode] Failed to open backend "
                  << receiver->BackendName() << ": "
                  << openResult.Error().Message() << std::endl;
        (void)ara::core::Deinitialize();
        return 1;
    }

    // 4) Configure CAN frame decoder for the expected IDs.
    sample::ara_com_socketcan_gateway::VehicleStatusCanDecoder decoder{
        sample::ara_com_socketcan_gateway::VehicleStatusCanDecoder::Config{
            config.PowertrainCanId,
            config.ChassisCanId,
            config.RequireBothFramesBeforeDecode}};

    {
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Started CAN template. backend=" << receiver->BackendName()
               << " ifname=" << config.CanInterface
               << " powertrain_can_id=0x" << std::hex << config.PowertrainCanId
               << " chassis_can_id=0x" << config.ChassisCanId << std::dec;
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 5) Poll CAN frames and decode them into a typed status structure.
    std::uint64_t rawFrameCount{0U};
    std::uint64_t decodedFrameCount{0U};
    while (gRunning.load())
    {
        sample::ara_com_socketcan_gateway::CanFrame canFrame{};
        auto receiveResult{
            receiver->Receive(
                canFrame,
                std::chrono::milliseconds{config.ReceiveTimeoutMs})};

        if (!receiveResult.HasValue())
        {
            auto stream{logger.WithLevel(ara::log::LogLevel::kWarn)};
            stream << "Receive error: " << receiveResult.Error().Message();
            logging->Log(logger, ara::log::LogLevel::kWarn, stream);
            std::this_thread::sleep_for(std::chrono::milliseconds{100U});
            continue;
        }

        if (!receiveResult.Value())
        {
            continue;
        }

        ++rawFrameCount;

        sample::vehicle_status::VehicleStatusFrame frame{};
        if (!decoder.TryDecode(canFrame, frame))
        {
            continue;
        }

        ++decodedFrameCount;
        auto stream{logger.WithLevel(ara::log::LogLevel::kInfo)};
        stream << "Decoded frame seq=" << frame.SequenceCounter
               << " speed_centi_kph=" << frame.SpeedCentiKph
               << " rpm=" << frame.EngineRpm
               << " gear=" << static_cast<std::uint32_t>(frame.Gear)
               << " raw_frames=" << static_cast<std::uint32_t>(rawFrameCount)
               << " decoded_frames=" << static_cast<std::uint32_t>(decodedFrameCount);
        logging->Log(logger, ara::log::LogLevel::kInfo, stream);
    }

    // 6) Shutdown in reverse order.
    receiver->Close();
    (void)ara::core::Deinitialize();
    return 0;
}
