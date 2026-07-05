#include <gtest/gtest.h>
#include "../../../src/ara/nm/network_manager.h"

namespace ara
{
    namespace nm
    {
        TEST(NetworkManagerTest, StateChangeHandlerCanQueryStatusDuringTick)
        {
            NetworkManager nm;
            ASSERT_TRUE(nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false}).HasValue());

            bool handlerCalled{false};
            ASSERT_TRUE(nm.SetStateChangeHandler(
                              [&nm, &handlerCalled](
                                  const std::string &channelName,
                                  NmState,
                                  NmState newState)
                              {
                                  handlerCalled = true;
                                  auto status = nm.GetChannelStatus(channelName);
                                  ASSERT_TRUE(status.HasValue());
                                  EXPECT_EQ(status.Value().State, newState);
                              })
                            .HasValue());

            ASSERT_TRUE(nm.NetworkRequest("ch1").HasValue());
            nm.Tick(1000U);

            EXPECT_TRUE(handlerCalled);
        }

        TEST(NetworkManagerTest, StateChangeHandlerCanQueryStatusDuringRepeatRequest)
        {
            NetworkManager nm;
            ASSERT_TRUE(nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false}).HasValue());
            ASSERT_TRUE(nm.NetworkRequest("ch1").HasValue());
            nm.Tick(1000U);
            nm.Tick(3000U);

            bool handlerCalled{false};
            ASSERT_TRUE(nm.SetStateChangeHandler(
                              [&nm, &handlerCalled](
                                  const std::string &channelName,
                                  NmState,
                                  NmState newState)
                              {
                                  handlerCalled = true;
                                  auto status = nm.GetChannelStatus(channelName);
                                  ASSERT_TRUE(status.HasValue());
                                  EXPECT_EQ(status.Value().State, newState);
                              })
                            .HasValue());

            ASSERT_TRUE(nm.HandleRepeatMessageRequest("ch1").HasValue());

            EXPECT_TRUE(handlerCalled);
        }

        TEST(NetworkManagerTest, RepeatRequestUsesLastTickTimeForDwell)
        {
            NetworkManager nm;
            ASSERT_TRUE(nm.AddChannel({"ch1", 5000U, 1500U, 2000U, false}).HasValue());
            ASSERT_TRUE(nm.NetworkRequest("ch1").HasValue());

            nm.Tick(1000U);
            nm.Tick(3000U);
            auto status = nm.GetChannelStatus("ch1");
            ASSERT_TRUE(status.HasValue());
            ASSERT_EQ(NmState::kNormalOperation, status.Value().State);

            ASSERT_TRUE(nm.HandleRepeatMessageRequest("ch1").HasValue());
            status = nm.GetChannelStatus("ch1");
            ASSERT_TRUE(status.HasValue());
            EXPECT_EQ(NmState::kRepeatMessage, status.Value().State);
            EXPECT_EQ(3000U, status.Value().StateEnteredEpochMs);

            nm.Tick(4000U);
            status = nm.GetChannelStatus("ch1");
            ASSERT_TRUE(status.HasValue());
            EXPECT_EQ(NmState::kRepeatMessage, status.Value().State);

            nm.Tick(4600U);
            status = nm.GetChannelStatus("ch1");
            ASSERT_TRUE(status.HasValue());
            EXPECT_EQ(NmState::kNormalOperation, status.Value().State);
        }
    }
}
