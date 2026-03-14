#include <gtest/gtest.h>
#include "../../../src/ara/com/field.h"
#include "../../../src/ara/com/serialization.h"
#include "../../../src/ara/core/promise.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        TEST(ProxyFieldTest, GetValue)
        {
            auto eventBinding = std::make_unique<test::MockProxyEventBinding>();
            auto getBinding = std::make_unique<test::MockProxyMethodBinding>();

            // Mock Get to return serialized uint32_t
            std::uint32_t expectedValue = 42U;
            getBinding->ResponseToReturn =
                Serializer<std::uint32_t>::Serialize(expectedValue);

            ProxyField<std::uint32_t> field{
                std::move(eventBinding),
                std::move(getBinding),
                nullptr,
                true,   // hasGetter
                false,  // hasSetter
                true    // hasNotifier
            };

            auto future = field.Get();
            auto result = future.GetResult();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 42U);
        }

        TEST(ProxyFieldTest, SetValue)
        {
            auto eventBinding = std::make_unique<test::MockProxyEventBinding>();
            auto setBinding = std::make_unique<test::MockProxyMethodBinding>();
            setBinding->ResponseToReturn = {};

            ProxyField<std::uint32_t> field{
                std::move(eventBinding),
                nullptr,
                std::move(setBinding),
                false,  // hasGetter
                true,   // hasSetter
                true    // hasNotifier
            };

            auto future = field.Set(100U);
            auto result = future.GetResult();
            EXPECT_TRUE(result.HasValue());
        }

        TEST(ProxyFieldTest, SubscribeAndGetNotifications)
        {
            auto eventBinding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawEventBinding = eventBinding.get();

            ProxyField<std::uint32_t> field{
                std::move(eventBinding),
                nullptr,
                nullptr,
                false,
                false,
                true
            };

            field.Subscribe(5);
            EXPECT_EQ(field.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);

            rawEventBinding->InjectSample(
                Serializer<std::uint32_t>::Serialize(999U));

            std::uint32_t received = 0;
            auto result = field.GetNewSamples(
                [&received](SamplePtr<std::uint32_t> sample)
                {
                    received = *sample;
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(received, 999U);
        }

        TEST(ProxyFieldTest, HasCapabilities)
        {
            ProxyField<int> field{
                std::make_unique<test::MockProxyEventBinding>(),
                std::make_unique<test::MockProxyMethodBinding>(),
                std::make_unique<test::MockProxyMethodBinding>(),
                true, true, true
            };

            EXPECT_TRUE(field.HasGetter());
            EXPECT_TRUE(field.HasSetter());
            EXPECT_TRUE(field.HasNotifier());
        }

        TEST(ProxyFieldTest, GetterDisabledReturnsError)
        {
            ProxyField<std::uint32_t> field{
                std::make_unique<test::MockProxyEventBinding>(),
                std::make_unique<test::MockProxyMethodBinding>(),
                nullptr,
                false,
                false,
                true};

            auto future = field.Get();
            auto result = future.GetResult();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyFieldTest, SetterDisabledReturnsError)
        {
            ProxyField<std::uint32_t> field{
                std::make_unique<test::MockProxyEventBinding>(),
                nullptr,
                std::make_unique<test::MockProxyMethodBinding>(),
                false,
                false,
                true};

            auto future = field.Set(55U);
            auto result = future.GetResult();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyFieldTest, NotifierDisabledReturnsErrorForSamples)
        {
            ProxyField<std::uint32_t> field{
                nullptr,
                nullptr,
                nullptr,
                false,
                false,
                false};

            auto sampleResult = field.GetNewSamples(
                [](SamplePtr<std::uint32_t>)
                {
                });
            EXPECT_FALSE(sampleResult.HasValue());
            EXPECT_EQ(field.GetFreeSampleCount(), 0U);
            EXPECT_EQ(
                field.GetSubscriptionState(),
                SubscriptionState::kNotSubscribed);
        }

        // ── SkeletonField Tests ──

        TEST(SkeletonFieldTest, UpdateAndGetValue)
        {
            auto binding = std::make_unique<test::MockSkeletonEventBinding>();
            auto *rawBinding = binding.get();

            SkeletonField<std::uint32_t> field{std::move(binding)};
            field.Offer();

            field.Update(42U);
            EXPECT_EQ(field.GetValue(), 42U);

            // Verify the binding received the serialized payload
            ASSERT_EQ(rawBinding->SentPayloads.size(), 1U);
        }

        TEST(SkeletonFieldTest, MultipleUpdates)
        {
            auto binding = std::make_unique<test::MockSkeletonEventBinding>();
            auto *rawBinding = binding.get();

            SkeletonField<int> field{std::move(binding)};
            field.Offer();

            field.Update(1);
            field.Update(2);
            field.Update(3);

            EXPECT_EQ(field.GetValue(), 3);
            EXPECT_EQ(rawBinding->SentPayloads.size(), 3U);
        }

        TEST(SkeletonFieldTest, RegisterGetHandlerNoBindingReturnsError)
        {
            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>()};

            auto result = field.RegisterGetHandler(
                []() -> core::Future<std::uint32_t>
                {
                    core::Promise<std::uint32_t> p;
                    p.set_value(0U);
                    return p.get_future();
                });

            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonFieldTest, RegisterGetHandlerDispatchesOnRequest)
        {
            auto getBinding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *rawGet = getBinding.get();

            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>(),
                std::move(getBinding)};
            field.Update(77U);

            auto result = field.RegisterGetHandler(
                []() -> core::Future<std::uint32_t>
                {
                    core::Promise<std::uint32_t> p;
                    p.set_value(77U);
                    return p.get_future();
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_TRUE(rawGet->IsRegistered());

            auto reply = rawGet->InvokeRequest({});
            ASSERT_TRUE(reply.HasValue());

            auto deser = Serializer<std::uint32_t>::Deserialize(
                reply.Value().data(), reply.Value().size());
            ASSERT_TRUE(deser.HasValue());
            EXPECT_EQ(deser.Value(), 77U);
        }

        TEST(SkeletonFieldTest, RegisterSetHandlerNoBindingReturnsError)
        {
            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>()};

            auto result = field.RegisterSetHandler(
                [](const std::uint32_t &v) -> core::Future<std::uint32_t>
                {
                    core::Promise<std::uint32_t> p;
                    p.set_value(v);
                    return p.get_future();
                });

            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonFieldTest, RegisterSetHandlerDispatchesAndUpdatesValue)
        {
            auto setBinding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *rawSet = setBinding.get();

            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>(),
                nullptr,
                std::move(setBinding)};

            auto result = field.RegisterSetHandler(
                [](const std::uint32_t &newVal) -> core::Future<std::uint32_t>
                {
                    core::Promise<std::uint32_t> p;
                    p.set_value(newVal);
                    return p.get_future();
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_TRUE(rawSet->IsRegistered());

            auto request = Serializer<std::uint32_t>::Serialize(123U);
            auto reply = rawSet->InvokeRequest(request);
            ASSERT_TRUE(reply.HasValue());

            EXPECT_EQ(field.GetValue(), 123U);

            auto deser = Serializer<std::uint32_t>::Deserialize(
                reply.Value().data(), reply.Value().size());
            ASSERT_TRUE(deser.HasValue());
            EXPECT_EQ(deser.Value(), 123U);
        }

        TEST(SkeletonFieldTest, RegisterGetHandlerNullHandlerReturnsError)
        {
            auto getBinding = std::make_unique<test::MockSkeletonMethodBinding>();
            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>(),
                std::move(getBinding)};

            auto result = field.RegisterGetHandler(
                std::function<core::Future<std::uint32_t>()>{nullptr});
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonFieldTest, RegisterSetHandlerNullHandlerReturnsError)
        {
            auto setBinding = std::make_unique<test::MockSkeletonMethodBinding>();
            SkeletonField<std::uint32_t> field{
                std::make_unique<test::MockSkeletonEventBinding>(),
                nullptr,
                std::move(setBinding)};

            auto result = field.RegisterSetHandler(
                std::function<core::Future<std::uint32_t>(const std::uint32_t &)>{nullptr});
            EXPECT_FALSE(result.HasValue());
        }
    }
}
