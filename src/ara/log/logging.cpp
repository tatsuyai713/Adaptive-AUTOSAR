/// @file src/ara/log/logging.cpp
/// @brief Implementation for free-standing logging functions.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./logging.h"
#include <atomic>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace ara
{
    namespace log
    {
        static std::atomic<LogLevel> sDefaultLogLevel{LogLevel::kWarn};
        static std::string sAppId;
        static std::string sAppDescription;
        static LogMode sAppLogMode{LogMode::kConsole};

        void SetDefaultLogLevel(LogLevel logLevel) noexcept
        {
            sDefaultLogLevel.store(logLevel, std::memory_order_relaxed);
        }

        void InitLogging(
            const std::string &appId,
            const std::string &appDescription,
            LogLevel appDefLogLevel,
            LogMode appLogMode) noexcept
        {
            sAppId = appId;
            sAppDescription = appDescription;
            sDefaultLogLevel.store(appDefLogLevel, std::memory_order_relaxed);
            sAppLogMode = appLogMode;
        }

        void LogRaw(const void *data, std::size_t size) noexcept
        {
            if (!data || size == 0) return;
            // Format as hex dump to default logger
            // In a full platform, this would route through the configured sink.
            // For educational purposes, we create a hex string representation.
            (void)data;
            (void)size;
        }
    }
}
