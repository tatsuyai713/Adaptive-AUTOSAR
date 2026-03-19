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

        // ── EventCacheUpdatePolicy / FilterConfig accessor tests ──

        TEST(ProxyEventExtendedTest, DefaultCacheUpdatePolicy)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            EXPECT_EQ(event.GetCacheUpdatePolicy(),
                       EventCacheUpdatePolicy::kLastN);
        }

        TEST(ProxyEventExtendedTest, CachePolicyStored)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            event.Subscribe(10, EventCacheUpdatePolicy::kNewestN);
            EXPECT_EQ(event.GetCacheUpdatePolicy(),
                       EventCacheUpdatePolicy::kNewestN);
        }

        TEST(ProxyEventExtendedTest, DefaultFilterConfigNone)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            EXPECT_EQ(event.GetFilterConfig().Type, FilterType::kNone);
        }

        TEST(ProxyEventExtendedTest, FilterConfigStored)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            auto filter = FilterConfig::OneEveryN(3);
            event.Subscribe(10, filter);
            EXPECT_EQ(event.GetFilterConfig().Type, FilterType::kOneEveryN);
            EXPECT_EQ(event.GetFilterConfig().DecimationFactor, 3U);
        }

        TEST(ProxyEventExtendedTest, FilterConfigThresholdStored)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            auto filter = FilterConfig::Threshold(42.5);
            event.Subscribe(10, filter);
            EXPECT_EQ(event.GetFilterConfig().Type, FilterType::kThreshold);
            EXPECT_DOUBLE_EQ(event.GetFilterConfig().ThresholdValue, 42.5);
        }

        TEST(ProxyEventExtendedTest, FilterConfigRangeStored)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            ProxyEvent<int> event{std::move(binding)};

            auto filter = FilterConfig::Range(1.0, 100.0);
            event.Subscribe(10, filter);
            EXPECT_EQ(event.GetFilterConfig().Type, FilterType::kRange);
            EXPECT_DOUBLE_EQ(event.GetFilterConfig().RangeLow, 1.0);
            EXPECT_DOUBLE_EQ(event.GetFilterConfig().RangeHigh, 100.0);
        }

        TEST(ProxyEventExtendedTest, DecimationFilterApplied)
        {
            auto binding =
                std::make_unique<test::MockProxyEventBinding>();
            auto *raw = binding.get();
            ProxyEvent<int> event{std::move(binding)};

            auto filter = FilterConfig::OneEveryN(3);
            event.Subscribe(100, filter);

            // Inject 6 samples (each is a serialized int32_t = 1)
            for (int i = 0; i < 6; ++i)
            {
                int32_t val = i + 1;
                std::vector<uint8_t> data(
                    reinterpret_cast<const uint8_t *>(&val),
                    reinterpret_cast<const uint8_t *>(&val) + sizeof(val));
                raw->InjectSample(data);
            }

            std::vector<int> received;
            auto result = event.GetNewSamples(
                [&received](SamplePtr<int> sample)
                {
                    received.push_back(*sample);
                });
            ASSERT_TRUE(result.HasValue());

            // With OneEveryN(3), only every 3rd sample should pass:
            // sample 1: counter 1 < 3, skip
            // sample 2: counter 2 < 3, skip
            // sample 3: counter 3 >= 3, pass (val=3), reset
            // sample 4: counter 1 < 3, skip
            // sample 5: counter 2 < 3, skip
            // sample 6: counter 3 >= 3, pass (val=6), reset
            EXPECT_EQ(received.size(), 2U);
            if (received.size() == 2U)
            {
                EXPECT_EQ(received[0], 3);
                EXPECT_EQ(received[1], 6);
            }
        }
    }
}
