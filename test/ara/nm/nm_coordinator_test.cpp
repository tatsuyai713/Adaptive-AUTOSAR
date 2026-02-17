#include <gtest/gtest.h>
#include "../../../src/ara/nm/nm_coordinator.h"

namespace ara
{
    namespace nm
    {
        TEST(NmCoordinatorTest, ConstructWithManager)
        {
            NetworkManager _nm;
            _nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false});
            _nm.AddChannel({"ch2", 5000U, 1500U, 2000U, false});

            NmCoordinator _coord{_nm, CoordinatorPolicy::kAllChannelsSleep};
            auto _status = _coord.GetStatus();
            EXPECT_EQ(_status.ActiveChannelCount, 2U);
        }

        TEST(NmCoordinatorTest, InitialStatusBusSleep)
        {
            NetworkManager _nm;
            _nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false});

            NmCoordinator _coord{_nm};
            auto _status = _coord.GetStatus();

            // Channels start in BusSleep
            EXPECT_TRUE(_status.CoordinatedSleepReady);
            EXPECT_EQ(_status.SleepReadyChannelCount, 1U);
        }

        TEST(NmCoordinatorTest, RequestCoordinatedWakeup)
        {
            NetworkManager _nm;
            _nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false});
            _nm.AddChannel({"ch2", 5000U, 1500U, 2000U, false});

            NmCoordinator _coord{_nm};

            auto _result = _coord.RequestCoordinatedWakeup();
            EXPECT_TRUE(_result.HasValue());

            // After wakeup request, channels should transition out of BusSleep
            _coord.Tick(1000U);

            // Channels are now in RepeatMessage (waking up)
            auto _ch1 = _nm.GetChannelStatus("ch1");
            ASSERT_TRUE(_ch1.HasValue());
            EXPECT_NE(_ch1.Value().State, NmState::kBusSleep);
        }

        TEST(NmCoordinatorTest, RequestCoordinatedSleep)
        {
            NetworkManager _nm;
            _nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false});

            NmCoordinator _coord{_nm};

            auto _result = _coord.RequestCoordinatedSleep();
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(NmCoordinatorTest, EmptyManagerFails)
        {
            NetworkManager _nm;
            NmCoordinator _coord{_nm};

            auto _result = _coord.RequestCoordinatedSleep();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NmCoordinatorTest, MajorityPolicy)
        {
            NetworkManager _nm;
            _nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false});
            _nm.AddChannel({"ch2", 5000U, 1500U, 2000U, false});
            _nm.AddChannel({"ch3", 5000U, 1500U, 2000U, false});

            NmCoordinator _coord{_nm, CoordinatorPolicy::kMajoritySleep};

            // All start in BusSleep, so majority (3/3) are asleep
            auto _status = _coord.GetStatus();
            EXPECT_TRUE(_status.CoordinatedSleepReady);
            EXPECT_EQ(_status.SleepReadyChannelCount, 3U);
        }
    }
}
