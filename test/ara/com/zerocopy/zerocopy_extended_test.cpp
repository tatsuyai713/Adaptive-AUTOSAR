/// @file test/ara/com/zerocopy/zerocopy_extended_test.cpp
/// @brief Unit tests for zero-copy extended features:
///        ZeroCopyConfig, Service Discovery, Port Introspection.

#include <gtest/gtest.h>
#include <ara/com/zerocopy/zerocopy_config.h>
#include <ara/com/zerocopy/zerocopy_service_discovery.h>
#include <ara/com/zerocopy/port_introspection.h>

using namespace ara::com::zerocopy;

namespace ara_com_zerocopy_extended_test
{
    // ── ZeroCopy Config Tests ─────────────────────────────────

    TEST(ZeroCopyConfigTest, DefaultSubscriberConfig)
    {
        SubscriberConfig cfg;
        EXPECT_EQ(cfg.QueueCapacity, cDefaultQueueCapacity);
        EXPECT_EQ(cfg.HistoryRequest, 0U);
        EXPECT_TRUE(cfg.SubscribeOnCreate);
        EXPECT_EQ(cfg.RuntimeName, cDefaultRuntimeName);
    }

    TEST(ZeroCopyConfigTest, DefaultPublisherConfig)
    {
        PublisherConfig cfg;
        EXPECT_EQ(cfg.HistoryCapacity, cDefaultHistoryCapacity);
        EXPECT_TRUE(cfg.OfferOnCreate);
        EXPECT_EQ(cfg.RuntimeName, cDefaultRuntimeName);
    }

    TEST(ZeroCopyConfigTest, DefaultTransportConfig)
    {
        auto cfg = DefaultZeroCopyConfig();
        EXPECT_EQ(cfg.Subscriber.QueueCapacity, 256U);
        EXPECT_EQ(cfg.Publisher.HistoryCapacity, 0U);
        EXPECT_EQ(cfg.MaxChunkPayloadSize, 0U);
        EXPECT_EQ(cfg.PollTimeout.count(), 50);
    }

    TEST(ZeroCopyConfigTest, CustomSubscriberConfig)
    {
        SubscriberConfig cfg;
        cfg.QueueCapacity = 512U;
        cfg.HistoryRequest = 10U;
        cfg.SubscribeOnCreate = false;
        cfg.RuntimeName = "my_app";
        EXPECT_EQ(cfg.QueueCapacity, 512U);
        EXPECT_EQ(cfg.HistoryRequest, 10U);
        EXPECT_FALSE(cfg.SubscribeOnCreate);
        EXPECT_EQ(cfg.RuntimeName, "my_app");
    }

    TEST(ZeroCopyConfigTest, CustomPublisherConfig)
    {
        PublisherConfig cfg;
        cfg.HistoryCapacity = 5U;
        cfg.OfferOnCreate = false;
        cfg.RuntimeName = "my_publisher";
        EXPECT_EQ(cfg.HistoryCapacity, 5U);
        EXPECT_FALSE(cfg.OfferOnCreate);
    }

    TEST(ZeroCopyConfigTest, TransportConfigPollTimeout)
    {
        ZeroCopyTransportConfig cfg;
        cfg.PollTimeout = std::chrono::milliseconds{100};
        EXPECT_EQ(cfg.PollTimeout.count(), 100);
    }

    TEST(ZeroCopyConfigTest, MaxChunkPayloadSize)
    {
        ZeroCopyTransportConfig cfg;
        cfg.MaxChunkPayloadSize = 65536U;
        EXPECT_EQ(cfg.MaxChunkPayloadSize, 65536U);
    }

    // ── Service Discovery Tests ───────────────────────────────

    TEST(ZeroCopyServiceDiscoveryTest, InitiallyEmpty)
    {
        ZeroCopyServiceDiscovery sd;
        EXPECT_TRUE(sd.GetOfferedServices().empty());
        EXPECT_TRUE(sd.GetFoundServices().empty());
    }

    TEST(ZeroCopyServiceDiscoveryTest, OfferService)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_01", "inst_01", "evt_01"};
        sd.OfferService(ch);

        auto offered = sd.GetOfferedServices();
        ASSERT_EQ(offered.size(), 1U);
        EXPECT_EQ(offered[0].Channel.Service, "svc_01");
        EXPECT_EQ(offered[0].State, ServiceAvailability::kAvailable);
    }

    TEST(ZeroCopyServiceDiscoveryTest, StopOfferService)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_02", "inst_02", "evt_02"};
        sd.OfferService(ch);
        EXPECT_TRUE(sd.IsServiceOffered(ch));

        sd.StopOfferService(ch);
        EXPECT_FALSE(sd.IsServiceOffered(ch));
        EXPECT_TRUE(sd.GetOfferedServices().empty());
    }

    TEST(ZeroCopyServiceDiscoveryTest, DuplicateOffer)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_03", "inst_03", "evt_03"};
        sd.OfferService(ch);
        sd.OfferService(ch); // Duplicate — should not add twice
        EXPECT_EQ(sd.GetOfferedServices().size(), 1U);
    }

    TEST(ZeroCopyServiceDiscoveryTest, ServiceFound)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_04", "inst_04", "evt_04"};
        sd.ServiceFound(ch);

        EXPECT_TRUE(sd.IsServiceFound(ch));
        EXPECT_EQ(sd.GetFoundServices().size(), 1U);
    }

    TEST(ZeroCopyServiceDiscoveryTest, ServiceLost)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_05", "inst_05", "evt_05"};
        sd.ServiceFound(ch);
        EXPECT_TRUE(sd.IsServiceFound(ch));

        sd.ServiceLost(ch);
        EXPECT_FALSE(sd.IsServiceFound(ch));
    }

    TEST(ZeroCopyServiceDiscoveryTest, AvailabilityCallback)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_06", "inst_06", "evt_06"};

        bool callbackInvoked = false;
        ServiceAvailability lastState = ServiceAvailability::kNotAvailable;

        sd.SetAvailabilityHandler(
            [&](const DiscoveredService &svc)
            {
                callbackInvoked = true;
                lastState = svc.State;
            });

        sd.OfferService(ch);
        EXPECT_TRUE(callbackInvoked);
        EXPECT_EQ(lastState, ServiceAvailability::kAvailable);

        callbackInvoked = false;
        sd.StopOfferService(ch);
        EXPECT_TRUE(callbackInvoked);
        EXPECT_EQ(lastState, ServiceAvailability::kNotAvailable);
    }

    TEST(ZeroCopyServiceDiscoveryTest, IsServiceOfferedFalse)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_xx", "inst_xx", "evt_xx"};
        EXPECT_FALSE(sd.IsServiceOffered(ch));
    }

    TEST(ZeroCopyServiceDiscoveryTest, IsServiceFoundFalse)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_yy", "inst_yy", "evt_yy"};
        EXPECT_FALSE(sd.IsServiceFound(ch));
    }

    TEST(ZeroCopyServiceDiscoveryTest, MultipleServices)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch1{"svc_a", "inst_a", "evt_a"};
        ChannelDescriptor ch2{"svc_b", "inst_b", "evt_b"};

        sd.OfferService(ch1);
        sd.OfferService(ch2);
        EXPECT_EQ(sd.GetOfferedServices().size(), 2U);
    }

    TEST(ZeroCopyServiceDiscoveryTest, ServiceReFound)
    {
        ZeroCopyServiceDiscovery sd;
        ChannelDescriptor ch{"svc_rf", "inst_rf", "evt_rf"};

        sd.ServiceFound(ch);
        sd.ServiceLost(ch);
        EXPECT_FALSE(sd.IsServiceFound(ch));

        sd.ServiceFound(ch);
        EXPECT_TRUE(sd.IsServiceFound(ch));
    }

    // ── Port Introspection Tests ──────────────────────────────

    TEST(PortIntrospectionTest, PortConnectionStateValues)
    {
        EXPECT_EQ(static_cast<uint8_t>(PortConnectionState::kNotConnected), 0U);
        EXPECT_EQ(static_cast<uint8_t>(PortConnectionState::kConnected), 2U);
        EXPECT_EQ(static_cast<uint8_t>(PortConnectionState::kWaitForOffer), 4U);
    }

    TEST(PortIntrospectionTest, PublisherPortInfoDefaults)
    {
        PublisherPortInfo info;
        EXPECT_EQ(info.SubscriberCount, 0U);
        EXPECT_EQ(info.HistoryCapacity, 0U);
        EXPECT_FALSE(info.IsOffered);
    }

    TEST(PortIntrospectionTest, SubscriberPortInfoDefaults)
    {
        SubscriberPortInfo info;
        EXPECT_EQ(info.QueueCapacity, 0U);
        EXPECT_EQ(info.ConnectionState, PortConnectionState::kNotConnected);
    }

    TEST(PortIntrospectionTest, MemoryPoolInfoFreeChunks)
    {
        MemoryPoolInfo pool;
        pool.TotalChunks = 100U;
        pool.UsedChunks = 30U;
        EXPECT_EQ(pool.FreeChunks(), 70U);
    }

    TEST(PortIntrospectionTest, MemoryPoolInfoUsageRatio)
    {
        MemoryPoolInfo pool;
        pool.TotalChunks = 100U;
        pool.UsedChunks = 50U;
        EXPECT_DOUBLE_EQ(pool.UsageRatio(), 0.5);
    }

    TEST(PortIntrospectionTest, MemoryPoolInfoEmpty)
    {
        MemoryPoolInfo pool;
        EXPECT_EQ(pool.FreeChunks(), 0U);
        EXPECT_DOUBLE_EQ(pool.UsageRatio(), 0.0);
    }

    TEST(PortIntrospectionTest, SharedMemoryInfoFreeBytes)
    {
        SharedMemoryInfo memInfo;
        memInfo.TotalBytes = 1024U * 1024U;
        memInfo.UsedBytes = 512U * 1024U;
        EXPECT_EQ(memInfo.FreeBytes(), 512U * 1024U);
    }

    TEST(PortIntrospectionTest, SharedMemoryInfoEmpty)
    {
        SharedMemoryInfo memInfo;
        EXPECT_EQ(memInfo.FreeBytes(), 0U);
    }

    TEST(PortIntrospectionTest, IntrospectionSnapshotDefaults)
    {
        IntrospectionSnapshot snapshot;
        EXPECT_TRUE(snapshot.Publishers.empty());
        EXPECT_TRUE(snapshot.Subscribers.empty());
        EXPECT_TRUE(snapshot.MemorySegments.empty());
    }

    TEST(PortIntrospectionTest, PublisherPortWithChannel)
    {
        PublisherPortInfo info;
        info.Channel = {"my_svc", "my_inst", "my_evt"};
        info.RuntimeName = "test_app";
        info.SubscriberCount = 3U;
        info.HistoryCapacity = 10U;
        info.IsOffered = true;

        EXPECT_EQ(info.Channel.Service, "my_svc");
        EXPECT_EQ(info.RuntimeName, "test_app");
        EXPECT_EQ(info.SubscriberCount, 3U);
        EXPECT_TRUE(info.IsOffered);
    }

    TEST(PortIntrospectionTest, MemoryPoolOverflowProtection)
    {
        MemoryPoolInfo pool;
        pool.TotalChunks = 10U;
        pool.UsedChunks = 20U; // More used than total (corruption scenario)
        EXPECT_EQ(pool.FreeChunks(), 0U); // Should not underflow
    }

    TEST(PortIntrospectionTest, ServiceAvailabilityEnum)
    {
        EXPECT_EQ(static_cast<uint8_t>(ServiceAvailability::kAvailable), 0U);
        EXPECT_EQ(static_cast<uint8_t>(ServiceAvailability::kNotAvailable), 1U);
    }
}
