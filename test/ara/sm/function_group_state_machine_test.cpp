#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "../../../src/ara/sm/function_group_state_machine.h"

namespace ara
{
    namespace sm
    {
        TEST(FunctionGroupStateMachineTest, AddStateAndSetCurrent)
        {
            FunctionGroupStateMachine _sm;

            auto _r1 = _sm.AddState("Off");
            EXPECT_TRUE(_r1.HasValue());

            auto _r2 = _sm.AddState("Running");
            EXPECT_TRUE(_r2.HasValue());

            auto _r3 = _sm.SetCurrentState("Off");
            EXPECT_TRUE(_r3.HasValue());

            EXPECT_EQ(_sm.GetCurrentState(), "Off");
        }

        TEST(FunctionGroupStateMachineTest, AddDuplicateStateFails)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Running");

            auto _result = _sm.AddState("Running");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(FunctionGroupStateMachineTest, AddEmptyStateFails)
        {
            FunctionGroupStateMachine _sm;
            auto _result = _sm.AddState("");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(FunctionGroupStateMachineTest, TryTransition)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Off");
            _sm.AddState("Running");
            _sm.SetCurrentState("Off");

            FunctionGroupTransition _trans;
            _trans.fromState = "Off";
            _trans.toState = "Running";
            _sm.AddTransition(_trans);

            auto _result = _sm.TryTransition("Running");
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(_sm.GetCurrentState(), "Running");
        }

        TEST(FunctionGroupStateMachineTest, TryTransitionGuardBlocks)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Off");
            _sm.AddState("Running");
            _sm.SetCurrentState("Off");

            FunctionGroupTransition _trans;
            _trans.fromState = "Off";
            _trans.toState = "Running";
            _trans.guardFn = []() { return false; };
            _sm.AddTransition(_trans);

            auto _result = _sm.TryTransition("Running");
            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(_sm.GetCurrentState(), "Off");
        }

        TEST(FunctionGroupStateMachineTest, TimeoutTransitionViaTick)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Off");
            _sm.AddState("Running");
            _sm.SetCurrentState("Off");

            FunctionGroupTransition _trans;
            _trans.fromState = "Off";
            _trans.toState = "Running";
            _trans.timeoutMs = 100U;
            _sm.AddTransition(_trans);

            // Tick at time < timeout: no transition
            auto now = std::chrono::steady_clock::now();
            _sm.Tick(now + std::chrono::milliseconds(50));
            EXPECT_EQ(_sm.GetCurrentState(), "Off");

            // Tick at time >= timeout: auto-transition fires
            _sm.Tick(now + std::chrono::milliseconds(200));
            EXPECT_EQ(_sm.GetCurrentState(), "Running");
        }

        TEST(FunctionGroupStateMachineTest, TransitionHistory)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Off");
            _sm.AddState("Running");
            _sm.AddState("Stopped");
            _sm.SetCurrentState("Off");

            _sm.AddTransition({"Off", "Running", nullptr, 0U});
            _sm.AddTransition({"Running", "Stopped", nullptr, 0U});

            _sm.TryTransition("Running");
            _sm.TryTransition("Stopped");

            auto _history = _sm.GetTransitionHistory();
            EXPECT_EQ(_history.size(), 2U);
            EXPECT_EQ(_history[0].fromState, "Off");
            EXPECT_EQ(_history[0].toState, "Running");
            EXPECT_EQ(_history[1].fromState, "Running");
            EXPECT_EQ(_history[1].toState, "Stopped");
        }

        TEST(FunctionGroupStateMachineTest, StateChangeCallback)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Off");
            _sm.AddState("Running");
            _sm.SetCurrentState("Off");
            _sm.AddTransition({"Off", "Running", nullptr, 0U});

            std::string _capturedFrom;
            std::string _capturedTo;
            _sm.SetStateChangeHandler(
                [&](const std::string &from, const std::string &to)
                {
                    _capturedFrom = from;
                    _capturedTo = to;
                });

            _sm.TryTransition("Running");
            EXPECT_EQ(_capturedFrom, "Off");
            EXPECT_EQ(_capturedTo, "Running");
        }

        TEST(FunctionGroupStateMachineTest, HasState)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("Running");
            EXPECT_TRUE(_sm.HasState("Running"));
            EXPECT_FALSE(_sm.HasState("Stopped"));
        }

        TEST(FunctionGroupStateMachineTest, WildcardFromState)
        {
            FunctionGroupStateMachine _sm;
            _sm.AddState("A");
            _sm.AddState("B");
            _sm.AddState("Error");
            _sm.SetCurrentState("B");

            // Empty fromState = wildcard (matches any state)
            _sm.AddTransition({"", "Error", nullptr, 0U});

            auto _result = _sm.TryTransition("Error");
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(_sm.GetCurrentState(), "Error");
        }
    }
}
