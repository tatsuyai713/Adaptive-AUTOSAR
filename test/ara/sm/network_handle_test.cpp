#include <gtest/gtest.h>
#include "../../../src/ara/sm/network_handle.h"

namespace ara
{
    namespace sm
    {
        TEST(NetworkHandleTest, InitialModeIsNone)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};

            auto _result = _handle.GetCurrentComMode();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), ComMode::kNone);
        }

        TEST(NetworkHandleTest, RequestComModeChangesMode)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};

            ASSERT_TRUE(_handle.RequestComMode(ComMode::kFull).HasValue());

            auto _result = _handle.GetCurrentComMode();
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), ComMode::kFull);
        }

        TEST(NetworkHandleTest, RequestSameModeReturnsError)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};

            ASSERT_TRUE(_handle.RequestComMode(ComMode::kFull).HasValue());

            auto _result = _handle.RequestComMode(ComMode::kFull);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "SM");
        }

        TEST(NetworkHandleTest, NotifierCalledOnModeChange)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};
            ComMode _captured{ComMode::kNone};
            int _callCount{0};

            _handle.SetNotifier(
                [&](ComMode mode)
                {
                    _captured = mode;
                    ++_callCount;
                });

            _handle.RequestComMode(ComMode::kFull);
            EXPECT_EQ(_captured, ComMode::kFull);
            EXPECT_EQ(_callCount, 1);

            _handle.RequestComMode(ComMode::kSilent);
            EXPECT_EQ(_captured, ComMode::kSilent);
            EXPECT_EQ(_callCount, 2);
        }

        TEST(NetworkHandleTest, ClearNotifierStopsCallbacks)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};
            int _callCount{0};

            _handle.SetNotifier([&](ComMode) { ++_callCount; });
            _handle.ClearNotifier();

            _handle.RequestComMode(ComMode::kFull);
            EXPECT_EQ(_callCount, 0);
        }

        TEST(NetworkHandleTest, SetEmptyNotifierFails)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};

            auto _result = _handle.SetNotifier(nullptr);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NetworkHandleTest, GetInstanceReturnsSpecifier)
        {
            core::InstanceSpecifier _spec{"net/eth0"};
            NetworkHandle _handle{_spec};

            EXPECT_EQ(_handle.GetInstance(), _spec);
        }
    }
}
