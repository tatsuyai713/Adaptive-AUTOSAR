#include <gtest/gtest.h>
#include "../../../src/ara/sm/machine_state_client.h"

namespace ara
{
    namespace sm
    {
        TEST(MachineStateClientTest, InitialStateIsStartup)
        {
            MachineStateClient _client;
            auto _result = _client.GetMachineState();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), MachineState::kStartup);
        }

        TEST(MachineStateClientTest, SetMachineStateChangesState)
        {
            MachineStateClient _client;
            ASSERT_TRUE(_client.SetMachineState(MachineState::kRunning).HasValue());

            auto _result = _client.GetMachineState();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), MachineState::kRunning);
        }

        TEST(MachineStateClientTest, SetSameStateReturnsError)
        {
            MachineStateClient _client;
            ASSERT_TRUE(_client.SetMachineState(MachineState::kRunning).HasValue());

            auto _result = _client.SetMachineState(MachineState::kRunning);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "SM");
        }

        TEST(MachineStateClientTest, NotifierCalledOnStateChange)
        {
            MachineStateClient _client;
            MachineState _captured{MachineState::kStartup};
            int _callCount{0};

            _client.SetNotifier(
                [&](MachineState state)
                {
                    _captured = state;
                    ++_callCount;
                });

            _client.SetMachineState(MachineState::kRunning);
            EXPECT_EQ(_captured, MachineState::kRunning);
            EXPECT_EQ(_callCount, 1);
        }

        TEST(MachineStateClientTest, NotifierCalledWithCorrectStateOnSecondChange)
        {
            MachineStateClient _client;
            MachineState _captured{MachineState::kStartup};
            int _callCount{0};

            _client.SetNotifier(
                [&](MachineState state)
                {
                    _captured = state;
                    ++_callCount;
                });

            _client.SetMachineState(MachineState::kRunning);
            _client.SetMachineState(MachineState::kShutdown);
            EXPECT_EQ(_captured, MachineState::kShutdown);
            EXPECT_EQ(_callCount, 2);
        }

        TEST(MachineStateClientTest, ClearNotifierStopsCallbacks)
        {
            MachineStateClient _client;
            int _callCount{0};

            _client.SetNotifier([&](MachineState) { ++_callCount; });
            _client.ClearNotifier();

            _client.SetMachineState(MachineState::kRunning);
            EXPECT_EQ(_callCount, 0);
        }

        TEST(MachineStateClientTest, SetEmptyNotifierFails)
        {
            MachineStateClient _client;
            auto _result = _client.SetNotifier(nullptr);
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
