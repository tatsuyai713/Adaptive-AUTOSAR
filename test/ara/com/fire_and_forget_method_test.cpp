/// @file test/ara/com/fire_and_forget_method_test.cpp
/// @brief Unit tests for ProxyFireAndForgetMethod and SkeletonFireAndForgetMethod.

#include <gtest/gtest.h>
#include "../../../src/ara/com/method.h"
#include "../../../src/ara/com/serialization.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        // ── ProxyFireAndForgetMethod tests ───────────────────────────────────

        TEST(ProxyFireAndForgetMethodTest, CallSerializesArgsAndSends)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            auto *raw = binding.get();

            ProxyFireAndForgetMethod<int, int> method{std::move(binding)};

            method(3, 4);

            // Args should be serialized and sent
            auto expected = Serializer<int>::Serialize(3);
            auto b2 = Serializer<int>::Serialize(4);
            expected.insert(expected.end(), b2.begin(), b2.end());

            EXPECT_EQ(raw->LastRequest, expected);
        }

        TEST(ProxyFireAndForgetMethodTest, CallWithNoBindingIsNoOp)
        {
            ProxyFireAndForgetMethod<int> method{nullptr};
            // Must not crash
            method(42);
        }

        TEST(ProxyFireAndForgetMethodTest, CallDoesNotBlockOnResponse)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            binding->ShouldFail = true; // binding will fail — caller must not care

            ProxyFireAndForgetMethod<int> method{std::move(binding)};
            // Fire-and-forget: call completes even when binding fails
            method(1);
        }

        TEST(ProxyFireAndForgetMethodTest, CallWithNoArgsSucceeds)
        {
            auto binding = std::make_unique<test::MockProxyMethodBinding>();
            auto *raw = binding.get();

            ProxyFireAndForgetMethod<> method{std::move(binding)};
            method();

            EXPECT_TRUE(raw->LastRequest.empty());
        }

        // ── SkeletonFireAndForgetMethod tests ────────────────────────────────

        TEST(SkeletonFireAndForgetMethodTest, SetHandlerInvokesCallable)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonFireAndForgetMethod<int, int> method{std::move(binding)};

            int capturedA = 0, capturedB = 0;
            auto regResult = method.SetHandler(
                [&](const int &a, const int &b) {
                    capturedA = a;
                    capturedB = b;
                });

            ASSERT_TRUE(regResult.HasValue());

            // Simulate incoming request
            auto request = Serializer<int>::Serialize(7);
            auto b2 = Serializer<int>::Serialize(13);
            request.insert(request.end(), b2.begin(), b2.end());

            auto response = raw->InvokeRequest(request);
            ASSERT_TRUE(response.HasValue());
            EXPECT_TRUE(response.Value().empty()); // no return payload

            EXPECT_EQ(capturedA, 7);
            EXPECT_EQ(capturedB, 13);
        }

        TEST(SkeletonFireAndForgetMethodTest, NoBindingReturnsError)
        {
            SkeletonFireAndForgetMethod<int> method{nullptr};

            auto result = method.SetHandler([](const int &) {});
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonFireAndForgetMethodTest, UnsetHandlerAfterSet)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonFireAndForgetMethod<int> method{std::move(binding)};

            method.SetHandler([](const int &) {});
            EXPECT_TRUE(raw->IsRegistered());

            method.UnsetHandler();
            EXPECT_FALSE(raw->IsRegistered());
        }

        TEST(SkeletonFireAndForgetMethodTest, SetHandlerNoArgsSucceeds)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonFireAndForgetMethod<> method{std::move(binding)};

            bool called = false;
            method.SetHandler([&]() { called = true; });

            auto response = raw->InvokeRequest({});
            ASSERT_TRUE(response.HasValue());
            EXPECT_TRUE(called);
        }

        TEST(SkeletonFireAndForgetMethodTest, BadPayloadReturnsError)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonFireAndForgetMethod<int> method{std::move(binding)};
            method.SetHandler([](const int &) {});

            // Pass too-short payload (int needs 4 bytes)
            auto response = raw->InvokeRequest({0x01, 0x02}); // only 2 bytes
            EXPECT_FALSE(response.HasValue());
        }

    } // namespace com
} // namespace ara
