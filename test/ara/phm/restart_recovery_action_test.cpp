#include <gtest/gtest.h>
#include "../../../src/ara/phm/restart_recovery_action.h"

namespace ara
{
    namespace phm
    {
        static const core::InstanceSpecifier cSpecifier("RestartInstance");

        TEST(RestartRecoveryActionTest, NullCallbackThrows)
        {
            EXPECT_THROW(
                RestartRecoveryAction _action(cSpecifier, nullptr),
                std::invalid_argument);
        }

        TEST(RestartRecoveryActionTest, HandlerInvokesCallbackWhenOffered)
        {
            bool _callbackInvoked{false};
            std::string _invokedInstance;

            RestartRecoveryAction _action{
                cSpecifier,
                [&](const core::InstanceSpecifier &instance)
                {
                    _callbackInvoked = true;
                    _invokedInstance = instance.ToString();
                }};

            _action.Offer();

            exec::ExecutionErrorEvent _event;
            _action.RecoveryHandler(_event, TypeOfSupervision::AliveSupervision);

            EXPECT_TRUE(_callbackInvoked);
            EXPECT_EQ(_invokedInstance, "RestartInstance");
        }

        TEST(RestartRecoveryActionTest, HandlerDoesNothingWhenNotOffered)
        {
            bool _callbackInvoked{false};

            RestartRecoveryAction _action{
                cSpecifier,
                [&](const core::InstanceSpecifier &)
                {
                    _callbackInvoked = true;
                }};

            exec::ExecutionErrorEvent _event;
            _action.RecoveryHandler(_event, TypeOfSupervision::AliveSupervision);

            EXPECT_FALSE(_callbackInvoked);
        }
    }
}
