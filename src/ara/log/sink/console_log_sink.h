/// @file src/ara/log/sink/console_log_sink.h
/// @brief Declarations for console log sink.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CONSOLE_LOG_SINK_H
#define CONSOLE_LOG_SINK_H

#include <iostream>
#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief Log sink implementation that writes logs to standard output.
            class ConsoleLogSink : public LogSink
            {
            public:
                /// @brief Constructor
                /// @param appId Application ID
                /// @param appDescription Application description
                ConsoleLogSink(
                    std::string appId,
                    std::string appDescription);

                ConsoleLogSink() = delete;
                void Log(const LogStream &logStream) const override;
            };
        }
    }
}

#endif
