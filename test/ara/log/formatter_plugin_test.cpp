#include <gtest/gtest.h>
#include "../../../src/ara/log/formatter_plugin.h"

namespace ara
{
    namespace log
    {
        TEST(FormatterPluginTest, DefaultFormatterOutput)
        {
            DefaultFormatter _fmt;
            LogEntry _entry;
            _entry.Level = LogLevel::kInfo;
            _entry.AppId = "APP1";
            _entry.CtxId = "CTX1";
            _entry.Message = "hello";
            auto _out = _fmt.Format(_entry);
            EXPECT_FALSE(_out.empty());
            EXPECT_NE(_out.find("APP1"), std::string::npos);
            EXPECT_NE(_out.find("hello"), std::string::npos);
        }

        TEST(FormatterPluginTest, DefaultFormatterName)
        {
            DefaultFormatter _fmt;
            EXPECT_NE(std::string(_fmt.Name()).size(), 0U);
        }

        TEST(FormatterPluginTest, JsonFormatterOutput)
        {
            JsonFormatter _fmt;
            LogEntry _entry;
            _entry.Level = LogLevel::kWarn;
            _entry.AppId = "APP2";
            _entry.CtxId = "CTX2";
            _entry.Message = "warn msg";
            auto _out = _fmt.Format(_entry);
            EXPECT_FALSE(_out.empty());
            // JSON should contain braces
            EXPECT_NE(_out.find("{"), std::string::npos);
            EXPECT_NE(_out.find("}"), std::string::npos);
        }

        TEST(FormatterPluginTest, JsonFormatterName)
        {
            JsonFormatter _fmt;
            EXPECT_NE(std::string(_fmt.Name()).size(), 0U);
        }

        TEST(FormatterPluginTest, RegistrySingleton)
        {
            auto &_a = FormatterRegistry::Instance();
            auto &_b = FormatterRegistry::Instance();
            EXPECT_EQ(&_a, &_b);
        }

        TEST(FormatterPluginTest, RegisterAndGet)
        {
            auto &_reg = FormatterRegistry::Instance();
            auto _plugin = std::make_shared<JsonFormatter>();
            _reg.Register(_plugin);
            EXPECT_TRUE(_reg.Has(std::string(_plugin->Name())));
            auto _got = _reg.Get(std::string(_plugin->Name()));
            EXPECT_NE(_got, nullptr);
        }

        TEST(FormatterPluginTest, GetUnknownReturnsDefault)
        {
            auto &_reg = FormatterRegistry::Instance();
            auto _got = _reg.Get("nonexistent_formatter");
            // Should return the default formatter, not nullptr
            EXPECT_NE(_got, nullptr);
        }

        TEST(FormatterPluginTest, GetRegisteredNames)
        {
            auto &_reg = FormatterRegistry::Instance();
            auto _names = _reg.GetRegisteredNames();
            // At least default should be registered
            EXPECT_FALSE(_names.empty());
        }
    }
}
