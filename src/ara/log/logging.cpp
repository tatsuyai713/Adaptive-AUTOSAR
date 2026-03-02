/// @file src/ara/log/logging.cpp
/// @brief Implementation for free-standing logging functions.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./logging.h"
#include <atomic>

namespace ara
{
    namespace log
    {
        static std::atomic<LogLevel> sDefaultLogLevel{LogLevel::kWarn};

        void SetDefaultLogLevel(LogLevel logLevel) noexcept
        {
            sDefaultLogLevel.store(logLevel, std::memory_order_relaxed);
        }
    }
}
