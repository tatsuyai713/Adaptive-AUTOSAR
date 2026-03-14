#include <gtest/gtest.h>
#include "../../../src/ara/com/event.h"
#include "../../../src/ara/com/qos.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        // ── ProxyEvent extended tests ──

        TEST(ProxyEventExtendedTest, SubscribeWithCachePolicy)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            // Should not throw; delegates to base Subscribe.
            event.Subscribe(10, EventCacheUpdatePolicy::kLastN);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);

            event.Unsubscribe();
            event.Subscribe(5, EventCacheUpdatePolicy::kNewestN);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);
        }

        TEST(ProxyEventExtendedTest, SubscribeWithFilter)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            auto filter = FilterConfig::Threshold(5.0);
            event.Subscribe(8, filter);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);
        }

        TEST(ProxyEventExtendedTest, SubscribeWithQoS)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            auto qos = EventQosProfile::Reliable(20);
            event.Subscribe(10, qos);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);
        }

        TEST(ProxyEventExtendedTest, SizedReceiveHandler)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            auto *raw = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10);
            std::size_t notifiedCount = 0U;
            SizedEventReceiveHandler handler =
                [&notifiedCount](std::size_t count)
            {
                notifiedCount += count;
            };
            event.SetReceiveHandler(handler);

            // Inject a sample to trigger the handler.
            std::vector<uint8_t> data{0x01, 0x00, 0x00, 0x00};
            raw->InjectSample(data);

            EXPECT_GT(notifiedCount, 0U);
        }

        TEST(ProxyEventExtendedTest, SizedReceiveHandlerNull)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            // Setting a null SizedEventReceiveHandler should not crash.
            SizedEventReceiveHandler nullHandler;
            event.SetReceiveHandler(nullHandler);
        }

        TEST(ProxyEventExtendedTest, ReinitBinding)
        {
            auto binding1 =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding1)};

            event.Subscribe(5);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);

            // Reinit with a new binding.
            auto binding2 =
                std::make_unique<test::MockProxyEventBinding>();
            event.Reinit(std::move(binding2));

            // After reinit, old subscription is gone.
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);
        }

        TEST(ProxyEventExtendedTest, ReinitWithNullptr)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            event.Reinit(nullptr);
            EXPECT_EQ(event.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);
        }

        // ── SkeletonEvent extended tests ──

        TEST(SkeletonEventExtendedTest, SubscriptionStateHandler)
        {
            auto binding =
                std::make_unique<test::MockSkeletonEventBinding>();
            SkeletonEvent<int> event{std::move(binding)};

            uint16_t lastClientId = 0U;
            bool lastSubscribed = false;
            event.SetSubscriptionStateChangeHandler(
                [&](uint16_t clientId, bool subscribed)
                {
                    lastClientId = clientId;
                    lastSubscribed = subscribed;
                });

            event.NotifySubscriptionStateChange(42, true);
            EXPECT_EQ(lastClientId, 42U);
            EXPECT_TRUE(lastSubscribed);

            event.NotifySubscriptionStateChange(42, false);
            EXPECT_FALSE(lastSubscribed);
        }

        TEST(SkeletonEventExtendedTest, UnsetSubscriptionHandler)
        {
            auto binding =
                std::make_unique<test::MockSkeletonEventBinding>();
            SkeletonEvent<int> event{std::move(binding)};

            bool called = false;
            event.SetSubscriptionStateChangeHandler(
                [&](uint16_t, bool)
                { called = true; });

            event.UnsetSubscriptionStateChangeHandler();
            event.NotifySubscriptionStateChange(1, true);
            // Handler was unset, so callback should NOT have been called.
            EXPECT_FALSE(called);
        }

        TEST(SkeletonEventExtendedTest, NoHandlerNotifyDoesNotCrash)
        {
            auto binding =
                std::make_unique<test::MockSkeletonEventBinding>();
            SkeletonEvent<int> event{std::move(binding)};

            // No handler set, should not crash.
            event.NotifySubscriptionStateChange(99, true);
        }
    }
}
