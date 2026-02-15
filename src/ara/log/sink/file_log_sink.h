/// @file src/ara/log/sink/file_log_sink.h
/// @brief Declarations for file log sink.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef FILE_LOG_SINK_H
#define FILE_LOG_SINK_H

#include <fstream>
#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief Log sink implementation that appends logs to a file.
            class FileLogSink : public LogSink
            {
            private:
                std::string mLogFilePath;

            public:
                /// @brief Constructor
                /// @param appId Application ID
                /// @param appDescription Application description
                /// @param logFilePath Logging file sink path
                FileLogSink(
                    std::string appId,
                    std::string appDescription,
                    std::string logFilePath);

                FileLogSink() = delete;
                void Log(const LogStream &logStream) const override;
            };
        }
    }
}

#endif
