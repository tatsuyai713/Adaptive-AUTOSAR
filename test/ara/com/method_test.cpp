#include <gtest/gtest.h>
#include "../../../src/ara/com/method.h"
#include "../../../src/ara/com/serialization.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        TEST(ProxyMethodTest, CallWithReturnValue)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();

            // Set up mock to return serialized int 8
            int responseValue = 8;
            binding->ResponseToReturn = Serializer<int>::Serialize(responseValue);

            ProxyMethod<int(int, int)> method{std::move(binding)};

            auto future = method(3, 5);
            auto result = future.GetResult();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 8);
        }

        TEST(ProxyMethodTest, CallWithFailure)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            binding->ShouldFail = true;

            ProxyMethod<int(int)> method{std::move(binding)};

            auto future = method(42);
            auto result = future.GetResult();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyMethodTest, VoidReturn)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            binding->ResponseToReturn = {};

            ProxyMethod<void(int)> method{std::move(binding)};

            auto future = method(42);
            auto result = future.GetResult();
            EXPECT_TRUE(result.HasValue());
        }

        TEST(ProxyMethodTest, VoidReturnFailurePropagatesError)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            binding->ShouldFail = true;

            ProxyMethod<void(int)> method{std::move(binding)};

            auto future = method(7);
            auto result = future.GetResult();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyMethodTest, NoArgsWithReturn)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();

            double responseValue = 3.14;
            binding->ResponseToReturn = Serializer<double>::Serialize(responseValue);

            ProxyMethod<double()> method{std::move(binding)};

            auto future = method();
            auto result = future.GetResult();
            ASSERT_TRUE(result.HasValue());
            EXPECT_DOUBLE_EQ(result.Value(), 3.14);
        }

        TEST(ProxyMethodTest, NullBindingReturnsError)
        {
            ProxyMethod<int(int)> method{nullptr};

            auto future = method(1);
            auto result = future.GetResult();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyMethodTest, RequestPayloadSerialized)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            auto *rawBinding = binding.get();
            binding->ResponseToReturn = Serializer<int>::Serialize(0);

            ProxyMethod<int(int, int)> method{std::move(binding)};
            method(100, 200);

            // Verify the request payload contains both serialized ints
            EXPECT_EQ(rawBinding->LastRequest.size(),
                       sizeof(int) * 2);
        }
    }
}
