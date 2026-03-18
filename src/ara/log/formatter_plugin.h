/// @file src/ara/log/formatter_plugin.h
/// @brief Pluggable log formatter interface for custom output formats.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef LOG_FORMATTER_PLUGIN_H
#define LOG_FORMATTER_PLUGIN_H

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "./common.h"

namespace ara
{
    namespace log
    {
        /// @brief Log entry data passed to formatter plugins.
        struct LogEntry
        {
            LogLevel Level{LogLevel::kInfo};
            std::string AppId;
            std::string CtxId;
            std::string Message;
            std::chrono::system_clock::time_point Timestamp;
        };

        /// @brief Abstract interface for custom log formatters.
        class FormatterPlugin
        {
        public:
            virtual ~FormatterPlugin() = default;

            /// @brief Format a log entry into a string.
            virtual std::string Format(const LogEntry &entry) const = 0;

            /// @brief Return the unique name of this formatter.
            virtual const char *Name() const noexcept = 0;
        };

        /// @brief Default formatter: "[LEVEL] AppId/CtxId: message".
        class DefaultFormatter final : public FormatterPlugin
        {
        public:
            std::string Format(const LogEntry &entry) const override;
            const char *Name() const noexcept override;
        };

        /// @brief JSON formatter: {"level":"...","app":"...","msg":"..."}.
        class JsonFormatter final : public FormatterPlugin
        {
        public:
            std::string Format(const LogEntry &entry) const override;
            const char *Name() const noexcept override;
        };

        /// @brief Registry for formatter plugins.
        class FormatterRegistry
        {
        public:
            /// @brief Get the singleton instance.
            static FormatterRegistry &Instance();

            /// @brief Register a formatter plugin by name.
            void Register(std::shared_ptr<FormatterPlugin> plugin);

            /// @brief Get a formatter by name (returns default if not found).
            std::shared_ptr<FormatterPlugin> Get(
                const std::string &name) const;

            /// @brief Check if a formatter is registered.
            bool Has(const std::string &name) const;

            /// @brief Get names of all registered formatters.
            std::vector<std::string> GetRegisteredNames() const;

        private:
            FormatterRegistry();
            mutable std::mutex mMutex;
            std::map<std::string, std::shared_ptr<FormatterPlugin>> mPlugins;
        };
    }
}

#endif
