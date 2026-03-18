#include <gtest/gtest.h>
#include "../../../src/ara/core/abort_handler.h"

namespace ara
{
    namespace core
    {
        TEST(AbortHandlerTest, Singleton)
        {
            auto &_a = AbortHandler::Instance();
            auto &_b = AbortHandler::Instance();
            EXPECT_EQ(&_a, &_b);
        }

        TEST(AbortHandlerTest, RegisterCallback)
        {
            auto &_handler = AbortHandler::Instance();
            size_t _initial = _handler.GetCallbackCount();
            auto _idx = _handler.RegisterCallback(
                [](const AbortInfo &) {});
            EXPECT_EQ(_handler.GetCallbackCount(), _initial + 1);
            _handler.UnregisterCallback(_idx);
        }

        TEST(AbortHandlerTest, UnregisterCallback)
        {
            auto &_handler = AbortHandler::Instance();
            auto _idx = _handler.RegisterCallback(
                [](const AbortInfo &) {});
            size_t _before = _handler.GetCallbackCount();
            _handler.UnregisterCallback(_idx);
            EXPECT_EQ(_handler.GetCallbackCount(), _before - 1);
        }

        TEST(AbortHandlerTest, InstallAndRestoreHandlers)
        {
            auto &_handler = AbortHandler::Instance();
            _handler.InstallSignalHandlers();
            EXPECT_TRUE(_handler.HandlersInstalled());
            _handler.RestoreDefaultHandlers();
            EXPECT_FALSE(_handler.HandlersInstalled());
        }

        TEST(AbortHandlerTest, TriggerAbortInvokesCallback)
        {
            auto &_handler = AbortHandler::Instance();
            bool _called = false;
            AbortReason _captured = AbortReason::kUnknown;
            auto _idx = _handler.RegisterCallback(
                [&](const AbortInfo &info) {
                    _called = true;
                    _captured = info.Reason;
                });
            _handler.TriggerAbort(AbortReason::kTermination, "test abort");
            EXPECT_TRUE(_called);
            EXPECT_EQ(_captured, AbortReason::kTermination);
            _handler.UnregisterCallback(_idx);
        }

        TEST(AbortHandlerTest, AbortInfoFields)
        {
            auto &_handler = AbortHandler::Instance();
            std::string _msg;
            auto _idx = _handler.RegisterCallback(
                [&](const AbortInfo &info) {
                    _msg = info.Message;
                });
            _handler.TriggerAbort(AbortReason::kUserAbort, "hello");
            EXPECT_EQ(_msg, "hello");
            _handler.UnregisterCallback(_idx);
        }
    }
}
