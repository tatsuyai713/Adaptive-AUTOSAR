#include <gtest/gtest.h>
#include <initializer_list>
#include "../../../src/ara/log/logger.h"

namespace ara
{
    namespace log
    {
        TEST(LoggerTest, LogLevelOff)
        {
            const std::string cCtxId{"CTX01"};
            const std::string cCtxDescription{"Default Test Context"};
            const LogLevel cLogLevel{LogLevel::kOff};

            Logger _logger =
                Logger::CreateLogger(cCtxId, cCtxDescription, cLogLevel);

            std::initializer_list<LogLevel>
                _logLevels{
                    LogLevel::kFatal,
                    LogLevel::kError,
                    LogLevel::kWarn,
                    LogLevel::kInfo,
                    LogLevel::kDebug,
                    LogLevel::kVerbose};

            for (auto logLevel : _logLevels)
            {
                bool _isEnabled = _logger.IsEnabled(logLevel);
                ASSERT_FALSE(_isEnabled);
            }
        }

        TEST(LoggerTest, LogLevelVerbose)
        {
            const std::string cCtxId{"CTX01"};
            const std::string cCtxDescription{"Default Test Context"};
            const LogLevel cLogLevel{LogLevel::kVerbose};

            Logger _logger =
                Logger::CreateLogger(cCtxId, cCtxDescription, cLogLevel);

            std::initializer_list<LogLevel>
                _logLevels{
                    LogLevel::kFatal,
                    LogLevel::kError,
                    LogLevel::kWarn,
                    LogLevel::kInfo,
                    LogLevel::kDebug};

            for (auto logLevel : _logLevels)
            {
                bool _isEnabled = _logger.IsEnabled(logLevel);
                ASSERT_TRUE(_isEnabled);
            }
        }

        TEST(LoggerTest, WithLevelFunction)
        {
            const std::string cCtxId{"CTX01"};
            const std::string cCtxDescription{"Default Test Context"};
            const LogLevel cLogLevel{LogLevel::kWarn};

            Logger _logger =
                Logger::CreateLogger(cCtxId, cCtxDescription, cLogLevel);

            LogStream _logStream = _logger.WithLevel(cLogLevel);
            std::string _logStreamString = _logStream.ToString();

            bool _hasId =
                _logStreamString.find(cCtxId) != std::string::npos;
            ASSERT_TRUE(_hasId);

            bool _hasDescription =
                _logStreamString.find(cCtxDescription) != std::string::npos;
            ASSERT_TRUE(_hasDescription);
        }

        TEST(LoggerTest, RuntimeLogLevelCanBeUpdated)
        {
            Logger _logger = Logger::CreateLogger(
                "CTX_RUNTIME",
                "Runtime level test",
                LogLevel::kOff);

            EXPECT_FALSE(_logger.IsEnabled(LogLevel::kFatal));

            _logger.SetLogLevel(LogLevel::kInfo);
            EXPECT_EQ(_logger.GetLogLevel(), LogLevel::kInfo);
            EXPECT_TRUE(_logger.IsEnabled(LogLevel::kFatal));
            EXPECT_TRUE(_logger.IsEnabled(LogLevel::kInfo));
            EXPECT_FALSE(_logger.IsEnabled(LogLevel::kVerbose));
        }

        TEST(LoggerTest, ContextMetadataAccessors)
        {
            const std::string cCtxId{"CTX_META"};
            const std::string cCtxDescription{"Metadata accessor test"};

            Logger _logger = Logger::CreateLogger(
                cCtxId,
                cCtxDescription,
                LogLevel::kWarn);

            EXPECT_EQ(_logger.GetContextId(), cCtxId);
            EXPECT_EQ(_logger.GetContextDescription(), cCtxDescription);
        }
    }
}
