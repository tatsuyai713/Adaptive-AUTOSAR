/// @file src/ara/log/logging.h
/// @brief Free-standing logging functions per AUTOSAR AP SWS_LOG.
/// @details Provides `ara::log::CreateLogger()` and `ara::log::SetDefaultLogLevel()`
///          as specified by the AUTOSAR Adaptive Platform Logging specification.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_LOG_LOGGING_H
#define ARA_LOG_LOGGING_H

#include <string>
#include <cstdint>
#include "./logger.h"
#include "./common.h"
#include "./argument.h"

namespace ara
{
    namespace log
    {
        /// @brief Create a Logger instance (SWS_LOG_00001).
        /// @param ctxId Short context identifier (max 4 characters by DLT convention)
        /// @param ctxDescription Human-readable description of the logging context
        /// @param ctxDefLogLevel Default severity threshold for the new logger
        /// @returns A Logger value
        inline Logger CreateLogger(
            const std::string &ctxId,
            const std::string &ctxDescription,
            LogLevel ctxDefLogLevel = LogLevel::kWarn)
        {
            return Logger::CreateLogger(ctxId, ctxDescription, ctxDefLogLevel);
        }

        /// @brief Set the default log level for newly created loggers (SWS_LOG_00002).
        /// @param logLevel New default severity threshold
        /// @note This affects future CreateLogger calls. Existing loggers are unchanged.
        ///       In a full platform deployment, this would persist in the logging-framework
        ///       singleton. Here we store it in a static for simplicity.
        void SetDefaultLogLevel(LogLevel logLevel) noexcept;

        /// @brief Initialize the logging framework (SWS_LOG_00003).
        /// @param appId Application identifier (max 4 characters by DLT convention)
        /// @param appDescription Human-readable description of the application
        /// @param appDefLogLevel Default severity threshold for the application
        /// @param appLogMode Logging output mode (bitmask of LogMode values)
        void InitLogging(
            const std::string &appId,
            const std::string &appDescription,
            LogLevel appDefLogLevel,
            LogMode appLogMode) noexcept;

        /// @brief Register application description alias (SWS_LOG_00004).
        inline void RegisterAppDescription(
            const std::string &appId,
            const std::string &appDescription,
            LogLevel appDefLogLevel = LogLevel::kWarn,
            LogMode appLogMode = LogMode::kConsole) noexcept
        {
            InitLogging(appId, appDescription, appDefLogLevel, appLogMode);
        }

        /// @brief Log raw binary data (SWS_LOG_00005).
        /// @param data Pointer to raw data bytes
        /// @param size Number of bytes to log
        void LogRaw(const void *data, std::size_t size) noexcept;

        /// @brief Create an Argument payload (SWS_LOG_00053).
        /// @tparam T Value type
        /// @param name Human-readable name
        /// @param value The payload value
        /// @param unit Optional measurement unit (empty by default)
        /// @returns An Argument<T> ready for LogStream insertion
        template <typename T>
        Argument<T> Arg(const char *name, T &&value, const char *unit = "")
        {
            return Argument<T>(std::forward<T>(value), name, unit);
        }

    } // namespace log
} // namespace ara

#endif // ARA_LOG_LOGGING_H
