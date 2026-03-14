#include <gtest/gtest.h>
#include "../../../src/ara/com/network_binding_base.h"

namespace ara
{
    namespace com
    {
        TEST(LocalIpcBindingTest, InitialState)
        {
            LocalIpcBinding binding;
            EXPECT_EQ(binding.GetBindingType(), BindingType::kIpc);
            EXPECT_EQ(binding.GetConnectionState(),
                       ConnectionState::kDisconnected);
            EXPECT_STREQ(binding.GetBindingName(), "LocalIPC");
            EXPECT_FALSE(binding.IsHealthy());
        }

        TEST(LocalIpcBindingTest, Initialize)
        {
            LocalIpcBinding binding;
            auto result = binding.Initialize();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(binding.GetConnectionState(),
                       ConnectionState::kConnected);
            EXPECT_TRUE(binding.IsHealthy());
        }

        TEST(LocalIpcBindingTest, Shutdown)
        {
            LocalIpcBinding binding;
            binding.Initialize();
            binding.Shutdown();
            EXPECT_EQ(binding.GetConnectionState(),
                       ConnectionState::kDisconnected);
            EXPECT_FALSE(binding.IsHealthy());
        }

        TEST(LocalIpcBindingTest, StateHandler)
        {
            LocalIpcBinding binding;
            std::vector<ConnectionState> states;
            binding.SetConnectionStateHandler(
                [&states](ConnectionState s)
                {
                    states.push_back(s);
                });

            binding.Initialize();
            binding.Shutdown();

            ASSERT_EQ(states.size(), 2U);
            EXPECT_EQ(states[0], ConnectionState::kConnected);
            EXPECT_EQ(states[1], ConnectionState::kDisconnected);
        }

        TEST(LocalIpcBindingTest, DefaultStatistics)
        {
            LocalIpcBinding binding;
            EXPECT_EQ(binding.GetStatistics(), "No statistics available");
        }

        TEST(BindingTypeTest, EnumValues)
        {
            EXPECT_EQ(static_cast<std::uint8_t>(BindingType::kSomeIp), 0U);
            EXPECT_EQ(static_cast<std::uint8_t>(BindingType::kDds), 1U);
            EXPECT_EQ(static_cast<std::uint8_t>(BindingType::kIpc), 2U);
            EXPECT_EQ(static_cast<std::uint8_t>(BindingType::kUserDefined), 3U);
        }

        TEST(ConnectionStateTest, EnumValues)
        {
            EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::kDisconnected), 0U);
            EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::kConnecting), 1U);
            EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::kConnected), 2U);
            EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::kDegraded), 3U);
            EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::kReconnecting), 4U);
        }
    }
}
