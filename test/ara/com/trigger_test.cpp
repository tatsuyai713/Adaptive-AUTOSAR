#include <gtest/gtest.h>
#include "../../../src/ara/com/trigger.h"

namespace ara
{
    namespace com
    {
        // ── ProxyTrigger Tests ──────────────────────────────

        TEST(ProxyTriggerTest, InitialState)
        {
            ProxyTrigger trigger;
            EXPECT_EQ(trigger.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);
            EXPECT_EQ(trigger.GetTriggerCount(), 0U);
        }

        TEST(ProxyTriggerTest, SubscribeAndUnsubscribe)
        {
            ProxyTrigger trigger;
            auto result = trigger.Subscribe();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(trigger.GetSubscriptionState(),
                       SubscriptionState::kSubscribed);

            trigger.Unsubscribe();
            EXPECT_EQ(trigger.GetSubscriptionState(),
                       SubscriptionState::kNotSubscribed);
        }

        TEST(ProxyTriggerTest, ReceiveTrigger)
        {
            ProxyTrigger trigger;
            trigger.Subscribe();

            int callCount = 0;
            trigger.SetReceiveHandler([&callCount]()
                                      { ++callCount; });

            trigger.OnTriggerReceived();
            trigger.OnTriggerReceived();
            trigger.OnTriggerReceived();

            EXPECT_EQ(callCount, 3);
            EXPECT_EQ(trigger.GetTriggerCount(), 3U);
        }

        TEST(ProxyTriggerTest, NoHandlerStillCounts)
        {
            ProxyTrigger trigger;
            trigger.Subscribe();

            trigger.OnTriggerReceived();
            trigger.OnTriggerReceived();

            EXPECT_EQ(trigger.GetTriggerCount(), 2U);
        }

        TEST(ProxyTriggerTest, IgnoredWhenNotSubscribed)
        {
            ProxyTrigger trigger;
            // Not subscribed
            trigger.OnTriggerReceived();
            EXPECT_EQ(trigger.GetTriggerCount(), 0U);
        }

        TEST(ProxyTriggerTest, UnsetReceiveHandler)
        {
            ProxyTrigger trigger;
            trigger.Subscribe();

            int callCount = 0;
            trigger.SetReceiveHandler([&callCount]()
                                      { ++callCount; });

            trigger.OnTriggerReceived();
            EXPECT_EQ(callCount, 1);

            trigger.UnsetReceiveHandler();
            trigger.OnTriggerReceived();
            EXPECT_EQ(callCount, 1); // Not incremented
            EXPECT_EQ(trigger.GetTriggerCount(), 2U); // But count still goes up
        }

        // ── SkeletonTrigger Tests ──────────────────────────

        TEST(SkeletonTriggerTest, InitialState)
        {
            SkeletonTrigger trigger;
            EXPECT_FALSE(trigger.IsOffered());
            EXPECT_EQ(trigger.GetSubscriberCount(), 0U);
        }

        TEST(SkeletonTriggerTest, OfferAndStopOffer)
        {
            SkeletonTrigger trigger;
            auto result = trigger.Offer();
            ASSERT_TRUE(result.HasValue());
            EXPECT_TRUE(trigger.IsOffered());

            trigger.StopOffer();
            EXPECT_FALSE(trigger.IsOffered());
        }

        TEST(SkeletonTriggerTest, FireNotifiesSubscribers)
        {
            SkeletonTrigger skeletonTrigger;
            skeletonTrigger.Offer();

            ProxyTrigger proxy1;
            ProxyTrigger proxy2;
            proxy1.Subscribe();
            proxy2.Subscribe();

            int count1 = 0, count2 = 0;
            proxy1.SetReceiveHandler([&count1]()
                                     { ++count1; });
            proxy2.SetReceiveHandler([&count2]()
                                     { ++count2; });

            skeletonTrigger.AddSubscriber(proxy1);
            skeletonTrigger.AddSubscriber(proxy2);

            auto fireResult = skeletonTrigger.Fire();
            ASSERT_TRUE(fireResult.HasValue());

            EXPECT_EQ(count1, 1);
            EXPECT_EQ(count2, 1);
            EXPECT_EQ(proxy1.GetTriggerCount(), 1U);
            EXPECT_EQ(proxy2.GetTriggerCount(), 1U);
        }

        TEST(SkeletonTriggerTest, FireWhenNotOfferedFails)
        {
            SkeletonTrigger trigger;
            auto result = trigger.Fire();
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonTriggerTest, AddSubscriberWhenNotOfferedFails)
        {
            SkeletonTrigger trigger;
            ProxyTrigger proxy;
            proxy.Subscribe();
            auto result = trigger.AddSubscriber(proxy);
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SkeletonTriggerTest, RemoveSubscriber)
        {
            SkeletonTrigger skeletonTrigger;
            skeletonTrigger.Offer();

            ProxyTrigger proxy;
            proxy.Subscribe();
            skeletonTrigger.AddSubscriber(proxy);
            EXPECT_EQ(skeletonTrigger.GetSubscriberCount(), 1U);

            skeletonTrigger.RemoveSubscriber(proxy);
            EXPECT_EQ(skeletonTrigger.GetSubscriberCount(), 0U);

            int callCount = 0;
            proxy.SetReceiveHandler([&callCount]()
                                    { ++callCount; });
            skeletonTrigger.Fire();
            EXPECT_EQ(callCount, 0); // Not notified after removal
        }

        TEST(SkeletonTriggerTest, SubscriberCount)
        {
            SkeletonTrigger skeletonTrigger;
            skeletonTrigger.Offer();

            ProxyTrigger p1, p2, p3;
            p1.Subscribe();
            p2.Subscribe();
            p3.Subscribe();

            skeletonTrigger.AddSubscriber(p1);
            skeletonTrigger.AddSubscriber(p2);
            skeletonTrigger.AddSubscriber(p3);
            EXPECT_EQ(skeletonTrigger.GetSubscriberCount(), 3U);
        }

        TEST(SkeletonTriggerTest, StopOfferClearsSubscribers)
        {
            SkeletonTrigger skeletonTrigger;
            skeletonTrigger.Offer();

            ProxyTrigger p1, p2;
            p1.Subscribe();
            p2.Subscribe();
            skeletonTrigger.AddSubscriber(p1);
            skeletonTrigger.AddSubscriber(p2);
            EXPECT_EQ(skeletonTrigger.GetSubscriberCount(), 2U);

            skeletonTrigger.StopOffer();
            EXPECT_EQ(skeletonTrigger.GetSubscriberCount(), 0U);
        }
    }
}
