#include <gtest/gtest.h>
#include "../../../src/ara/sm/state_transition_handler.h"

namespace ara
{
    namespace sm
    {
        TEST(StateTransitionHandlerTest, RegisterAndHasHandler)
        {
            StateTransitionHandler _handler;
            auto _result = _handler.Register(
                "MachineFG",
                [](const std::string &, const std::string &,
                   const std::string &, TransitionPhase) {});
            ASSERT_TRUE(_result.HasValue());
            EXPECT_TRUE(_handler.HasHandler("MachineFG"));
            EXPECT_FALSE(_handler.HasHandler("OtherFG"));
        }

        TEST(StateTransitionHandlerTest, NotifyBeforePhase)
        {
            StateTransitionHandler _handler;
            std::string _capturedFg;
            std::string _capturedFrom;
            std::string _capturedTo;
            TransitionPhase _capturedPhase{TransitionPhase::kAfter};

            _handler.Register(
                "MachineFG",
                [&](const std::string &fg, const std::string &from,
                    const std::string &to, TransitionPhase phase)
                {
                    _capturedFg = fg;
                    _capturedFrom = from;
                    _capturedTo = to;
                    _capturedPhase = phase;
                });

            _handler.NotifyTransition(
                "MachineFG", "Off", "Running", TransitionPhase::kBefore);

            EXPECT_EQ(_capturedFg, "MachineFG");
            EXPECT_EQ(_capturedFrom, "Off");
            EXPECT_EQ(_capturedTo, "Running");
            EXPECT_EQ(_capturedPhase, TransitionPhase::kBefore);
        }

        TEST(StateTransitionHandlerTest, NotifyAfterPhase)
        {
            StateTransitionHandler _handler;
            TransitionPhase _capturedPhase{TransitionPhase::kBefore};

            _handler.Register(
                "MachineFG",
                [&](const std::string &, const std::string &,
                    const std::string &, TransitionPhase phase)
                {
                    _capturedPhase = phase;
                });

            _handler.NotifyTransition(
                "MachineFG", "Off", "Running", TransitionPhase::kAfter);

            EXPECT_EQ(_capturedPhase, TransitionPhase::kAfter);
        }

        TEST(StateTransitionHandlerTest, UnregisterRemovesCallback)
        {
            StateTransitionHandler _handler;
            int _callCount{0};

            _handler.Register(
                "MachineFG",
                [&](const std::string &, const std::string &,
                    const std::string &, TransitionPhase)
                {
                    ++_callCount;
                });

            _handler.Unregister("MachineFG");
            EXPECT_FALSE(_handler.HasHandler("MachineFG"));

            _handler.NotifyTransition(
                "MachineFG", "Off", "Running", TransitionPhase::kAfter);
            EXPECT_EQ(_callCount, 0);
        }

        TEST(StateTransitionHandlerTest, NotifyUnregisteredGroupIsSafe)
        {
            StateTransitionHandler _handler;
            // Should not throw or crash
            _handler.NotifyTransition(
                "NonExistent", "A", "B", TransitionPhase::kBefore);
        }

        TEST(StateTransitionHandlerTest, RegisterEmptyGroupFails)
        {
            StateTransitionHandler _handler;
            auto _result = _handler.Register(
                "",
                [](const std::string &, const std::string &,
                   const std::string &, TransitionPhase) {});
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
