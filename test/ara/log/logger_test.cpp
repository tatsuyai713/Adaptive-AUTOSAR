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

        TEST(LoggerTest, WithLevelFiltersDisabledLevel)
        {
            Logger _logger = Logger::CreateLogger(
                "CTX_FILTER", "Filter test", LogLevel::kWarn);

            // kInfo is above kWarn threshold, so disabled
            LogStream _stream = _logger.WithLevel(LogLevel::kInfo);
            EXPECT_TRUE(_stream.ToString().empty());

            // kDebug and kVerbose should also be empty
            LogStream _debug = _logger.WithLevel(LogLevel::kDebug);
            EXPECT_TRUE(_debug.ToString().empty());

            LogStream _verbose = _logger.WithLevel(LogLevel::kVerbose);
            EXPECT_TRUE(_verbose.ToString().empty());
        }

        TEST(LoggerTest, WithLevelPassesEnabledLevel)
        {
            Logger _logger = Logger::CreateLogger(
                "CTX_PASS", "Pass test", LogLevel::kWarn);

            // kFatal, kError, kWarn should all produce output
            LogStream _fatal = _logger.WithLevel(LogLevel::kFatal);
            EXPECT_FALSE(_fatal.ToString().empty());
            EXPECT_NE(_fatal.ToString().find("CTX_PASS"), std::string::npos);

            LogStream _error = _logger.WithLevel(LogLevel::kError);
            EXPECT_FALSE(_error.ToString().empty());

            LogStream _warn = _logger.WithLevel(LogLevel::kWarn);
            EXPECT_FALSE(_warn.ToString().empty());
        }

        TEST(LoggerTest, LogOffProducesEmptyStreams)
        {
            Logger _logger = Logger::CreateLogger(
                "CTX_OFF", "Off test", LogLevel::kOff);

            // Even Fatal should produce empty stream when level is kOff
            LogStream _fatal = _logger.LogFatal();
            EXPECT_TRUE(_fatal.ToString().empty());

            LogStream _error = _logger.LogError();
            EXPECT_TRUE(_error.ToString().empty());
        }

        TEST(LoggerTest, SetLogLevelAffectsWithLevel)
        {
            Logger _logger = Logger::CreateLogger(
                "CTX_DYN", "Dynamic test", LogLevel::kError);

            // kInfo not enabled at kError level
            LogStream _info1 = _logger.WithLevel(LogLevel::kInfo);
            EXPECT_TRUE(_info1.ToString().empty());

            // Raise level to kVerbose
            _logger.SetLogLevel(LogLevel::kVerbose);
            LogStream _info2 = _logger.WithLevel(LogLevel::kInfo);
            EXPECT_FALSE(_info2.ToString().empty());
        }
    }
}
