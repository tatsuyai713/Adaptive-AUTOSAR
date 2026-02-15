#include <gtest/gtest.h>
#include "../../../src/ara/phm/recovery_action_dispatcher.h"
#include "../../../src/ara/phm/recovery_action.h"
#include "../../../src/ara/phm/phm_error_domain.h"

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

            EXPECT_TRUE(_dispatcher.Register("action1", &_action).HasValue());
            EXPECT_EQ(_dispatcher.GetActionCount(), 1U);

            exec::ExecutionErrorEvent _event;
            EXPECT_TRUE(
                _dispatcher.Dispatch(
                    "action1", _event, TypeOfSupervision::AliveSupervision)
                    .HasValue());
            EXPECT_EQ(_action.HandlerCallCount, 1);
        }

        TEST(RecoveryActionDispatcherTest, DuplicateRegisterFails)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;

            EXPECT_TRUE(_dispatcher.Register("dup", &_action).HasValue());
            auto _duplicateRegisterResult{
                _dispatcher.Register("dup", &_action)};
            EXPECT_FALSE(_duplicateRegisterResult.HasValue());
            EXPECT_EQ(
                PhmErrc::kAlreadyExists,
                static_cast<PhmErrc>(_duplicateRegisterResult.Error().Value()));
            EXPECT_EQ(_dispatcher.GetActionCount(), 1U);
        }

        TEST(RecoveryActionDispatcherTest, UnregisterRemovesAction)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;

            ASSERT_TRUE(_dispatcher.Register("removable", &_action).HasValue());
            EXPECT_TRUE(_dispatcher.Unregister("removable").HasValue());
            EXPECT_EQ(_dispatcher.GetActionCount(), 0U);
            auto _missingUnregisterResult{
                _dispatcher.Unregister("removable")};
            EXPECT_FALSE(_missingUnregisterResult.HasValue());
            EXPECT_EQ(
                PhmErrc::kNotFound,
                static_cast<PhmErrc>(_missingUnregisterResult.Error().Value()));
        }

        TEST(RecoveryActionDispatcherTest, DispatchUnknownReturnsFalse)
        {
            RecoveryActionDispatcher _dispatcher;
            exec::ExecutionErrorEvent _event;

            auto _dispatchResult{
                _dispatcher.Dispatch(
                    "nonexistent", _event, TypeOfSupervision::DeadlineSupervision)};
            EXPECT_FALSE(_dispatchResult.HasValue());
            EXPECT_EQ(
                PhmErrc::kNotFound,
                static_cast<PhmErrc>(_dispatchResult.Error().Value()));
        }

        TEST(RecoveryActionDispatcherTest, RegisterNullptrFails)
        {
            RecoveryActionDispatcher _dispatcher;
            auto _registerResult{
                _dispatcher.Register("null", nullptr)};
            EXPECT_FALSE(_registerResult.HasValue());
            EXPECT_EQ(
                PhmErrc::kInvalidArgument,
                static_cast<PhmErrc>(_registerResult.Error().Value()));
        }

        TEST(RecoveryActionDispatcherTest, RegisterEmptyNameFails)
        {
            MockRecoveryAction _action;
            RecoveryActionDispatcher _dispatcher;
            auto _registerResult{
                _dispatcher.Register("", &_action)};
            EXPECT_FALSE(_registerResult.HasValue());
            EXPECT_EQ(
                PhmErrc::kInvalidArgument,
                static_cast<PhmErrc>(_registerResult.Error().Value()));
        }
    }
}
