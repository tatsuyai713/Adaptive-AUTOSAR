#include <cstring>
#include <gtest/gtest.h>
#include "../../../src/ara/com/event.h"
#include "../../../src/ara/com/serialization.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        // ── ProxyEvent Tests ──────────────────────────────

        TEST(ProxyEventTest, SubscribeAndUnsubscribe)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);

            event.Subscribe(10);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);

            event.Unsubscribe();
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);
        }

        TEST(ProxyEventTest, GetNewSamplesTyped)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);

            // Inject a serialized int sample
            int testValue = 42;
            auto serialized = Serializer<int>::Serialize(testValue);
            rawBinding->InjectSample(serialized);

            // Retrieve with typed callback
            int received = 0;
            auto result = event.GetNewSamples(
                [&received](SamplePtr<int> sample)
                {
                    received = *sample;
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 1U);
            EXPECT_EQ(received, 42);
        }

        TEST(ProxyEventTest, GetMultipleSamples)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);

            rawBinding->InjectSample(Serializer<int>::Serialize(10));
            rawBinding->InjectSample(Serializer<int>::Serialize(20));
            rawBinding->InjectSample(Serializer<int>::Serialize(30));

            std::vector<int> received;
            auto result = event.GetNewSamples(
                [&received](SamplePtr<int> sample)
                {
                    received.push_back(*sample);
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 3U);
            ASSERT_EQ(received.size(), 3U);
            EXPECT_EQ(received[0], 10);
            EXPECT_EQ(received[1], 20);
            EXPECT_EQ(received[2], 30);
        }

        TEST(ProxyEventTest, DeserializeFailureReturnsError)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);
            rawBinding->InjectSample(std::vector<std::uint8_t>{0x01});

            auto result = event.GetNewSamples(
                [](SamplePtr<int>)
                {
                });
            EXPECT_FALSE(result.HasValue());
        }

        TEST(ProxyEventTest, GetNewSamplesWithLimit)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);

            rawBinding->InjectSample(Serializer<int>::Serialize(1));
            rawBinding->InjectSample(Serializer<int>::Serialize(2));
            rawBinding->InjectSample(Serializer<int>::Serialize(3));

            std::vector<int> received;
            auto result = event.GetNewSamples(
                [&received](SamplePtr<int> sample)
                {
                    received.push_back(*sample);
                },
                2U);

            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 2U);
            EXPECT_EQ(received.size(), 2U);
        }

        struct TestStruct
        {
            std::uint32_t Id;
            double Value;
        };

        TEST(ProxyEventTest, StructSample)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<TestStruct> event{std::move(binding)};

            event.Subscribe(5);

            TestStruct original{123U, 3.14};
            rawBinding->InjectSample(Serializer<TestStruct>::Serialize(original));

            TestStruct received{};
            auto result = event.GetNewSamples(
                [&received](SamplePtr<TestStruct> sample)
                {
                    received = *sample;
                });

            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(received.Id, 123U);
            EXPECT_DOUBLE_EQ(received.Value, 3.14);
        }

        TEST(ProxyEventTest, GetFreeSampleCount)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);
            EXPECT_EQ(event.GetFreeSampleCount(), 10U);
        }

        TEST(ProxyEventTest, ReceiveHandler)
        {
            auto binding = std::make_unique<test::MockProxyEventBinding>();
            auto *rawBinding = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);

            bool handlerCalled = false;
            event.SetReceiveHandler([&handlerCalled]()
            {
                handlerCalled = true;
            });

            rawBinding->InjectSample(Serializer<int>::Serialize(99));
            EXPECT_TRUE(handlerCalled);

            event.UnsetReceiveHandler();
        }

        // ── SkeletonEvent Tests ──────────────────────────

        TEST(SkeletonEventTest, SendByCopy)
        {
            auto binding = std::make_unique<test::MockSkeletonEventBinding>();
            auto *rawBinding = binding.get();
            SkeletonEvent<int> event{std::move(binding)};

            event.Offer();
            EXPECT_TRUE(rawBinding->IsOffered());

            event.Send(42);
            ASSERT_EQ(rawBinding->SentPayloads.size(), 1U);

            auto deserResult = Serializer<int>::Deserialize(
                rawBinding->SentPayloads[0].data(),
                rawBinding->SentPayloads[0].size());
            ASSERT_TRUE(deserResult.HasValue());
            EXPECT_EQ(deserResult.Value(), 42);
        }

        TEST(SkeletonEventTest, SendStructByCopy)
        {
            auto binding = std::make_unique<test::MockSkeletonEventBinding>();
            auto *rawBinding = binding.get();
            SkeletonEvent<TestStruct> event{std::move(binding)};

            event.Offer();

            TestStruct data{456U, 2.718};
            event.Send(data);

            ASSERT_EQ(rawBinding->SentPayloads.size(), 1U);
            auto deserResult = Serializer<TestStruct>::Deserialize(
                rawBinding->SentPayloads[0].data(),
                rawBinding->SentPayloads[0].size());
            ASSERT_TRUE(deserResult.HasValue());
            EXPECT_EQ(deserResult.Value().Id, 456U);
            EXPECT_DOUBLE_EQ(deserResult.Value().Value, 2.718);
        }

        TEST(SkeletonEventTest, OfferAndStopOffer)
        {
            auto binding = std::make_unique<test::MockSkeletonEventBinding>();
            auto *rawBinding = binding.get();
            SkeletonEvent<int> event{std::move(binding)};

            EXPECT_FALSE(rawBinding->IsOffered());
            event.Offer();
            EXPECT_TRUE(rawBinding->IsOffered());
            event.StopOffer();
            EXPECT_FALSE(rawBinding->IsOffered());
        }
    }
}
