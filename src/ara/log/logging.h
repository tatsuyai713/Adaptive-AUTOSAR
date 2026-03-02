/// @file src/ara/log/logging.h
/// @brief Free-standing logging functions per AUTOSAR AP SWS_LOG.
/// @details Provides `ara::log::CreateLogger()` and `ara::log::SetDefaultLogLevel()`
///          as specified by the AUTOSAR Adaptive Platform Logging specification.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_LOG_LOGGING_H
#define ARA_LOG_LOGGING_H

#include <string>
#include "./logger.h"
#include "./common.h"

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

    } // namespace log
} // namespace ara

#endif // ARA_LOG_LOGGING_H
