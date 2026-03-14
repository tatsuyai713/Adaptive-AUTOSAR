#include <gtest/gtest.h>
#include "../../../src/ara/com/qos.h"

namespace ara
{
    namespace com
    {
        TEST(EventQosProfileTest, DefaultProfile)
        {
            EventQosProfile p;
            EXPECT_EQ(p.Reliability, ReliabilityKind::kBestEffort);
            EXPECT_EQ(p.History, HistoryKind::kKeepLast);
            EXPECT_EQ(p.HistoryDepth, 1U);
            EXPECT_EQ(p.Durability, DurabilityKind::kVolatile);
            EXPECT_EQ(p.Deadline.count(), 0);
            EXPECT_EQ(p.MinSeparation.count(), 0);
            EXPECT_EQ(p.Priority, 0U);
        }

        TEST(EventQosProfileTest, BestEffortFactory)
        {
            auto p = EventQosProfile::BestEffort();
            EXPECT_EQ(p.Reliability, ReliabilityKind::kBestEffort);
            EXPECT_EQ(p.HistoryDepth, 1U);
        }

        TEST(EventQosProfileTest, ReliableFactory)
        {
            auto p = EventQosProfile::Reliable(20);
            EXPECT_EQ(p.Reliability, ReliabilityKind::kReliable);
            EXPECT_EQ(p.HistoryDepth, 20U);
        }

        TEST(EventQosProfileTest, ReliableWithDeadline)
        {
            auto p = EventQosProfile::ReliableWithDeadline(
                std::chrono::milliseconds{100}, 5);
            EXPECT_EQ(p.Reliability, ReliabilityKind::kReliable);
            EXPECT_EQ(p.HistoryDepth, 5U);
            EXPECT_EQ(p.Deadline.count(), 100);
        }

        TEST(EventQosProfileTest, CustomConfiguration)
        {
            EventQosProfile p;
            p.Reliability = ReliabilityKind::kReliable;
            p.History = HistoryKind::kKeepAll;
            p.Durability = DurabilityKind::kTransientLocal;
            p.MinSeparation = std::chrono::milliseconds{10};
            p.Priority = 5U;

            EXPECT_EQ(p.Reliability, ReliabilityKind::kReliable);
            EXPECT_EQ(p.History, HistoryKind::kKeepAll);
            EXPECT_EQ(p.Durability, DurabilityKind::kTransientLocal);
            EXPECT_EQ(p.MinSeparation.count(), 10);
            EXPECT_EQ(p.Priority, 5U);
        }

        TEST(MethodQosProfileTest, DefaultProfile)
        {
            MethodQosProfile p;
            EXPECT_EQ(p.ResponseTimeout.count(), 5000);
            EXPECT_EQ(p.MaxConcurrentRequests, 16U);
            EXPECT_FALSE(p.AutoRetry);
            EXPECT_EQ(p.MaxRetries, 0U);
            EXPECT_EQ(p.RetryDelay.count(), 100);
            EXPECT_EQ(p.Priority, 0U);
        }

        TEST(MethodQosProfileTest, CustomConfiguration)
        {
            MethodQosProfile p;
            p.ResponseTimeout = std::chrono::milliseconds{2000};
            p.MaxConcurrentRequests = 4U;
            p.AutoRetry = true;
            p.MaxRetries = 3U;
            p.RetryDelay = std::chrono::milliseconds{500};
            p.Priority = 10U;

            EXPECT_EQ(p.ResponseTimeout.count(), 2000);
            EXPECT_EQ(p.MaxConcurrentRequests, 4U);
            EXPECT_TRUE(p.AutoRetry);
            EXPECT_EQ(p.MaxRetries, 3U);
            EXPECT_EQ(p.RetryDelay.count(), 500);
            EXPECT_EQ(p.Priority, 10U);
        }
    }
}
