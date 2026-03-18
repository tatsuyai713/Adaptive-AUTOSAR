/// @file src/ara/log/formatter_plugin.cpp
/// @brief Implementation of formatter plugins and registry.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./formatter_plugin.h"
#include <ctime>
#include <sstream>

namespace ara
{
    namespace log
    {
        // ─── DefaultFormatter ──────────────────────────────────────────

        static const char *LevelToString(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::kFatal:   return "FATAL";
            case LogLevel::kError:   return "ERROR";
            case LogLevel::kWarn:    return "WARN";
            case LogLevel::kInfo:    return "INFO";
            case LogLevel::kDebug:   return "DEBUG";
            case LogLevel::kVerbose: return "VERBOSE";
            default:                 return "OFF";
            }
        }

        std::string DefaultFormatter::Format(const LogEntry &entry) const
        {
            std::ostringstream oss;
            oss << "[" << LevelToString(entry.Level) << "] "
                << entry.AppId << "/" << entry.CtxId
                << ": " << entry.Message;
            return oss.str();
        }

        const char *DefaultFormatter::Name() const noexcept
        {
            return "default";
        }

        // ─── JsonFormatter ──────────────────────────────────────────────

        static std::string EscapeJson(const std::string &s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
            {
                switch (c)
                {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
                }
            }
            return out;
        }

        std::string JsonFormatter::Format(const LogEntry &entry) const
        {
            std::ostringstream oss;
            oss << "{\"level\":\"" << LevelToString(entry.Level) << "\""
                << ",\"app\":\"" << EscapeJson(entry.AppId) << "\""
                << ",\"ctx\":\"" << EscapeJson(entry.CtxId) << "\""
                << ",\"msg\":\"" << EscapeJson(entry.Message) << "\""
                << "}";
            return oss.str();
        }

        const char *JsonFormatter::Name() const noexcept
        {
            return "json";
        }

        // ─── FormatterRegistry ──────────────────────────────────────────

        FormatterRegistry &FormatterRegistry::Instance()
        {
            static FormatterRegistry instance;
            return instance;
        }

        FormatterRegistry::FormatterRegistry()
        {
            // Register built-in formatters.
            auto def = std::make_shared<DefaultFormatter>();
            auto json = std::make_shared<JsonFormatter>();
            mPlugins["default"] = std::move(def);
            mPlugins["json"] = std::move(json);
        }

        void FormatterRegistry::Register(
            std::shared_ptr<FormatterPlugin> plugin)
        {
            if (!plugin) return;
            std::lock_guard<std::mutex> lock(mMutex);
            mPlugins[plugin->Name()] = std::move(plugin);
        }

        std::shared_ptr<FormatterPlugin> FormatterRegistry::Get(
            const std::string &name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mPlugins.find(name);
            if (it != mPlugins.end()) return it->second;
            // Fallback to default.
            auto def = mPlugins.find("default");
            if (def != mPlugins.end()) return def->second;
            return nullptr;
        }

        bool FormatterRegistry::Has(const std::string &name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mPlugins.count(name) > 0;
        }

        std::vector<std::string> FormatterRegistry::GetRegisteredNames() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<std::string> names;
            names.reserve(mPlugins.size());
            for (const auto &kv : mPlugins)
            {
                names.push_back(kv.first);
            }
            return names;
        }
    }
}
