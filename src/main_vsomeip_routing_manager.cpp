/// @file src/main_vsomeip_routing_manager.cpp
/// @brief Entry point for the standalone vSomeIP routing manager daemon.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vsomeip/vsomeip.hpp>

namespace
{
    std::atomic_bool gRunning{true};
    std::mutex gWaitMutex;
    std::condition_variable gWaitCV;

    void RequestStop(int) noexcept
    {
        gRunning = false;
        gWaitCV.notify_all();
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
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string cAppName{
        GetEnvOrDefault(
            "AUTOSAR_VSOMEIP_ROUTING_APP",
            "autosar_vsomeip_routing_manager")};

    if (std::getenv("VSOMEIP_ROUTING") == nullptr)
    {
        setenv("VSOMEIP_ROUTING", cAppName.c_str(), 0);
    }

    std::shared_ptr<vsomeip::application> application{
        vsomeip::runtime::get()->create_application(cAppName)};
    if (!application)
    {
        std::cerr << "[ERROR] Failed to create vsomeip application: "
                  << cAppName << '\n';
        return 1;
    }

    application->register_state_handler(
        [](vsomeip::state_type_e state)
        {
            if (state == vsomeip::state_type_e::ST_REGISTERED)
            {
                std::cout << "[INFO] vSomeIP routing manager registered." << '\n';
            }
        });

    if (!application->init())
    {
        std::cerr << "[ERROR] Failed to initialize vsomeip application: "
                  << cAppName << '\n';
        return 1;
    }

    std::future<void> runFuture{
        std::async(
            std::launch::async,
            [application]()
            {
                application->start();
            })};

    {
        std::unique_lock<std::mutex> lock(gWaitMutex);
        gWaitCV.wait(lock, [] { return !gRunning.load(); });
    }

    application->stop();
    runFuture.wait();
    return 0;
}
