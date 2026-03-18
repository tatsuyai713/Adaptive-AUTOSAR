/// @file src/ara/log/logger.cpp
/// @brief Implementation for logger.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./logger.h"
#include <sys/stat.h>

namespace ara
{
    namespace log
    {
        Logger::Logger(std::string ctxId,
                       std::string ctxDescription,
                       LogLevel ctxDefLogLevel) : mContextId{ctxId},
                                                  mContextDescription{ctxDescription},
                                                  mContextDefaultLogLevel{ctxDefLogLevel}
        {
        }

        ClientState Logger::RemoteClientState() const noexcept
        {
            // Check whether a DLT daemon Unix-domain socket is present.
            // Standard DLT daemon socket paths on Linux/QNX.
            static const char *const cDltSocketPaths[] = {
                "/var/run/dlt",
                "/tmp/dlt",
                nullptr
            };
            for (const char *const *p = cDltSocketPaths; *p != nullptr; ++p)
            {
                struct stat st;
                if (::stat(*p, &st) == 0 && S_ISSOCK(st.st_mode))
                {
                    return ClientState::kConnected;
                }
            }
            return ClientState::kNotConnected;
        }

        LogStream Logger::LogFatal() const
        {
            LogStream _result = WithLevel(LogLevel::kFatal);
            return _result;
        }
        
        LogStream Logger::LogError() const
        {
            LogStream _result = WithLevel(LogLevel::kError);
            return _result;
        }

        LogStream Logger::LogWarn() const
        {
            LogStream _result = WithLevel(LogLevel::kWarn);
            return _result;
        }

        LogStream Logger::LogInfo() const
        {
            LogStream _result = WithLevel(LogLevel::kInfo);
            return _result;
        }

        LogStream Logger::LogDebug() const
        {
            LogStream _result = WithLevel(LogLevel::kDebug);
            return _result;
        }

        LogStream Logger::LogVerbose() const
        {
            LogStream _result = WithLevel(LogLevel::kVerbose);
            return _result;
        }

        bool Logger::IsEnabled(LogLevel logLevel) const noexcept
        {
            // Log levels are sorted in descending order
            bool _result = (logLevel <= mContextDefaultLogLevel);
            return _result;
        }

        void Logger::SetLogLevel(LogLevel logLevel) noexcept
        {
            mContextDefaultLogLevel = logLevel;
        }

        LogLevel Logger::GetLogLevel() const noexcept
        {
            return mContextDefaultLogLevel;
        }

        const std::string &Logger::GetContextId() const noexcept
        {
            return mContextId;
        }

        const std::string &Logger::GetContextDescription() const noexcept
        {
            return mContextDescription;
        }

        LogStream Logger::WithLevel(LogLevel logLevel) const
        {
            LogStream _result;

            if (!IsEnabled(logLevel))
            {
                return _result;
            }

            const std::string cContextId{"Context ID:"};
            const std::string cContextDescription{"Context Description:"};
            const std::string cLogLevel{"Log Level:"};
            const std::string cSeperator{";"};

            _result << cContextId << mContextId << cSeperator;
            _result << cContextDescription << mContextDescription << cSeperator;
            _result << cLogLevel << logLevel << cSeperator;

            return _result;
        }

        Logger Logger::CreateLogger(
            std::string ctxId,
            std::string ctxDescription,
            LogLevel ctxDefLogLevel)
        {
            Logger _result(ctxId, ctxDescription, ctxDefLogLevel);
            return _result;
        }
    }
}
