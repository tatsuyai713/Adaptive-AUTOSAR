#include <gtest/gtest.h>
#include "../../../src/ara/phm/recovery_action_dispatcher.h"
#include "../../../src/ara/phm/recovery_action.h"

namespace ara
{
    namespace phm
    {
        static const core::InstanceSpecifier cDispatcherSpec("DispatcherInstance");

        class MockRecoveryAction : public RecoveryAction
        {
        public:
            int HandlerCallCount{0};

            MockRecoveryAction() : RecoveryAction{cDispatcherSpec}
            {
            }

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override
            {
                ++HandlerCallCount;
            }
        };

        TEST(RecoveryActionDispatcherTest, RegisterAndDispatch)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;

            EXPECT_TRUE(_dispatcher.Register("action1", &_action));
            EXPECT_EQ(_dispatcher.GetActionCount(), 1U);

            exec::ExecutionErrorEvent _event;
            EXPECT_TRUE(
                _dispatcher.Dispatch(
                    "action1", _event, TypeOfSupervision::AliveSupervision));
            EXPECT_EQ(_action.HandlerCallCount, 1);
        }

        TEST(RecoveryActionDispatcherTest, DuplicateRegisterFails)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;

            EXPECT_TRUE(_dispatcher.Register("dup", &_action));
            EXPECT_FALSE(_dispatcher.Register("dup", &_action));
            EXPECT_EQ(_dispatcher.GetActionCount(), 1U);
        }

        TEST(RecoveryActionDispatcherTest, UnregisterRemovesAction)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;

            _dispatcher.Register("removable", &_action);
            EXPECT_TRUE(_dispatcher.Unregister("removable"));
            EXPECT_EQ(_dispatcher.GetActionCount(), 0U);
            EXPECT_FALSE(_dispatcher.Unregister("removable"));
        }

        TEST(RecoveryActionDispatcherTest, DispatchUnknownReturnsFalse)
        {
            RecoveryActionDispatcher _dispatcher;
            exec::ExecutionErrorEvent _event;

            EXPECT_FALSE(
                _dispatcher.Dispatch(
                    "nonexistent", _event, TypeOfSupervision::DeadlineSupervision));
        }

        TEST(RecoveryActionDispatcherTest, RegisterNullptrFails)
        {
            RecoveryActionDispatcher _dispatcher;
            EXPECT_FALSE(_dispatcher.Register("null", nullptr));
        }

        TEST(RecoveryActionDispatcherTest, RegisterEmptyNameFails)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;
            EXPECT_FALSE(_dispatcher.Register("", &_action));
        }
    }
}
