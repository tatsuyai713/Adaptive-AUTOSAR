#include <gtest/gtest.h>
#include "../../../src/ara/com/types.h"
#include "../../../src/ara/com/service_handle_type.h"

namespace ara
{
    namespace com
    {
        TEST(SubscriptionStateTest, EnumValues)
        {
            EXPECT_EQ(
                static_cast<std::uint8_t>(SubscriptionState::kNotSubscribed), 0U);
            EXPECT_EQ(
                static_cast<std::uint8_t>(SubscriptionState::kSubscriptionPending), 1U);
            EXPECT_EQ(
                static_cast<std::uint8_t>(SubscriptionState::kSubscribed), 2U);
        }

        TEST(MethodCallProcessingModeTest, EnumValues)
        {
            EXPECT_EQ(
                static_cast<std::uint8_t>(MethodCallProcessingMode::kPoll), 0U);
            EXPECT_EQ(
                static_cast<std::uint8_t>(MethodCallProcessingMode::kEvent), 1U);
            EXPECT_EQ(
                static_cast<std::uint8_t>(MethodCallProcessingMode::kEventSingleThread), 2U);
        }

        TEST(FindServiceHandleTest, Construction)
        {
            FindServiceHandle handle{42U};
            EXPECT_EQ(handle.GetId(), 42U);
        }

        TEST(FindServiceHandleTest, Equality)
        {
            FindServiceHandle a{1U};
            FindServiceHandle b{1U};
            FindServiceHandle c{2U};

            EXPECT_TRUE(a == b);
            EXPECT_FALSE(a == c);
            EXPECT_FALSE(a != b);
            EXPECT_TRUE(a != c);
        }

        TEST(ServiceHandleTypeTest, LessThanOperator)
        {
            ServiceHandleType a{0x1000, 0x0001};
            ServiceHandleType b{0x1000, 0x0002};
            ServiceHandleType c{0x2000, 0x0001};

            EXPECT_TRUE(a < b);
            EXPECT_TRUE(a < c);
            EXPECT_TRUE(b < c);
            EXPECT_FALSE(b < a);
            EXPECT_FALSE(a < a);
        }

        TEST(ServiceHandleContainerTest, VectorAlias)
        {
            ServiceHandleContainer<ServiceHandleType> handles;
            handles.emplace_back(0x1234, 0x0001);
            handles.emplace_back(0x1234, 0x0002);

            EXPECT_EQ(handles.size(), 2U);
            EXPECT_EQ(handles[0].GetInstanceId(), 0x0001);
            EXPECT_EQ(handles[1].GetInstanceId(), 0x0002);
        }
    }
}
