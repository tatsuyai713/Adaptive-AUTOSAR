#include <iostream>
#include <memory>

#include <ara/core/initialization.h>
#include <ara/log/logging_framework.h>

int main()
{
    // 1) Initialize AUTOSAR runtime before using ara::* APIs.
    auto initResult = ara::core::Initialize();
    if (!initResult.HasValue())
    {
        std::cerr << "[UserMinimal] Initialize failed: "
                  << initResult.Error().Message() << std::endl;
        return 1;
    }

    // 2) Create logging framework instance.
    auto logging = std::unique_ptr<ara::log::LoggingFramework>{
        ara::log::LoggingFramework::Create(
            "UAPP",
            ara::log::LogMode::kConsole,
            ara::log::LogLevel::kInfo,
            "Installed AUTOSAR AP user app demo")};

    // 3) Create one logger context under the framework.
    const auto &logger = logging->CreateLogger(
        "MINI",
        "Minimal runtime app",
        ara::log::LogLevel::kInfo);

    // 4) Emit one startup log entry.
    auto stream = logger.WithLevel(ara::log::LogLevel::kInfo);
    stream << "User app started using installed AUTOSAR AP libraries.";
    logging->Log(logger, ara::log::LogLevel::kInfo, stream);

    // 5) Deinitialize runtime during graceful shutdown.
    (void)ara::core::Deinitialize();
    return 0;
}
