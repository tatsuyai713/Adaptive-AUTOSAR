/// @file src/main.cpp
/// @brief Entry point for the platform-side Adaptive AUTOSAR process stack.

#include <atomic>
#include <cstdio>
#include <csignal>
#include <future>
#include <memory>
#include <thread>
#include <unistd.h>
#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

namespace
{
    std::atomic_bool gRunning{false};
    std::atomic_bool gStopRequested{false};
    AsyncBsdSocketLib::Poller gPoller;
    std::unique_ptr<application::platform::ExecutionManagement> gExecutionManagement;

    void RequestStop(int) noexcept
    {
        gStopRequested = true;
        gRunning = false;
    }

    void PerformPolling()
    {
        const std::chrono::milliseconds cSleepDuration{
            ara::exec::DeterministicClient::cCycleDelayMs};

        while (gRunning.load())
        {
            gPoller.TryPoll();
            std::this_thread::sleep_for(cSleepDuration);
        }
    }
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    application::helper::ArgumentConfiguration argumentConfiguration(argc, argv);

    gExecutionManagement.reset(
        new application::platform::ExecutionManagement(&gPoller));
    gExecutionManagement->Initialize(argumentConfiguration.GetArguments());

    gRunning = true;
    std::future<void> pollFuture{std::async(std::launch::async, PerformPolling)};

    if (isatty(STDIN_FILENO) == 1)
    {
        std::getchar();
        RequestStop(0);
    }
    else
    {
        while (!gStopRequested.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    const int result{gExecutionManagement->Terminate()};
    gRunning = false;
    pollFuture.get();
    gExecutionManagement.reset();
    return result;
}
