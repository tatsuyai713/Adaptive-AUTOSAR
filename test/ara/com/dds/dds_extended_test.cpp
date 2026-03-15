/// @file test/ara/com/dds/dds_extended_test.cpp
/// @brief Unit tests for DDS extended features:
///        Participant Factory, Status Conditions, Content Filter,
///        Extended QoS policies.

#include <gtest/gtest.h>
#include <ara/com/dds/dds_participant_factory.h>
#include <ara/com/dds/dds_status_condition.h>
#include <ara/com/dds/dds_content_filter.h>
#include <ara/com/dds/dds_qos_config.h>

using namespace ara::com::dds;

namespace ara_com_dds_extended_test
{
    // ── Participant Factory Tests ─────────────────────────────

    TEST(DdsParticipantFactoryTest, Singleton)
    {
        auto &f1 = DdsParticipantFactory::Instance();
        auto &f2 = DdsParticipantFactory::Instance();
        EXPECT_EQ(&f1, &f2);
    }

    TEST(DdsParticipantFactoryTest, AcquireCreatesParticipant)
    {
        auto &factory = DdsParticipantFactory::Instance();
        // Use a unique domain to avoid interfering with other tests
        const std::uint32_t domain = 200U;
        auto participant = factory.AcquireParticipant(domain);
        EXPECT_NE(participant, 0);
        EXPECT_EQ(factory.GetRefCount(domain), 1U);
        factory.ReleaseParticipant(domain);
        EXPECT_EQ(factory.GetRefCount(domain), 0U);
    }

    TEST(DdsParticipantFactoryTest, SharedParticipant)
    {
        auto &factory = DdsParticipantFactory::Instance();
        const std::uint32_t domain = 201U;
        auto p1 = factory.AcquireParticipant(domain);
        auto p2 = factory.AcquireParticipant(domain);
        EXPECT_EQ(p1, p2);
        EXPECT_EQ(factory.GetRefCount(domain), 2U);

        factory.ReleaseParticipant(domain);
        EXPECT_EQ(factory.GetRefCount(domain), 1U);
        factory.ReleaseParticipant(domain);
        EXPECT_EQ(factory.GetRefCount(domain), 0U);
    }

    TEST(DdsParticipantFactoryTest, ActiveDomainCount)
    {
        auto &factory = DdsParticipantFactory::Instance();
        const std::uint32_t d1 = 202U;
        const std::uint32_t d2 = 203U;

        auto before = factory.ActiveDomainCount();
        factory.AcquireParticipant(d1);
        factory.AcquireParticipant(d2);
        EXPECT_EQ(factory.ActiveDomainCount(), before + 2);

        factory.ReleaseParticipant(d1);
        factory.ReleaseParticipant(d2);
    }

    TEST(DdsParticipantFactoryTest, ReleaseNonExisting)
    {
        auto &factory = DdsParticipantFactory::Instance();
        // Should not crash
        factory.ReleaseParticipant(999U);
        EXPECT_EQ(factory.GetRefCount(999U), 0U);
    }

    TEST(DdsParticipantFactoryTest, UnusedDomainRefCountZero)
    {
        auto &factory = DdsParticipantFactory::Instance();
        EXPECT_EQ(factory.GetRefCount(998U), 0U);
    }

    // ── DDS Status Condition Tests ────────────────────────────

    TEST(DdsStatusConditionTest, StatusKindCombine)
    {
        auto combined = DdsStatusKind::kSampleLost |
                        DdsStatusKind::kSampleRejected;
        EXPECT_TRUE(HasStatus(combined, DdsStatusKind::kSampleLost));
        EXPECT_TRUE(HasStatus(combined, DdsStatusKind::kSampleRejected));
        EXPECT_FALSE(HasStatus(combined, DdsStatusKind::kLivelinessLost));
    }

    TEST(DdsStatusConditionTest, StatusKindNone)
    {
        auto none = DdsStatusKind::kNone;
        EXPECT_FALSE(HasStatus(none, DdsStatusKind::kSampleLost));
        EXPECT_FALSE(HasStatus(none, DdsStatusKind::kDataAvailable));
    }

    TEST(DdsStatusConditionTest, AllStatusFlags)
    {
        auto all =
            DdsStatusKind::kInconsistentTopic |
            DdsStatusKind::kOfferedDeadlineMissed |
            DdsStatusKind::kRequestedDeadlineMissed |
            DdsStatusKind::kOfferedIncompatibleQos |
            DdsStatusKind::kRequestedIncompatibleQos |
            DdsStatusKind::kSampleLost |
            DdsStatusKind::kSampleRejected |
            DdsStatusKind::kDataOnReaders |
            DdsStatusKind::kDataAvailable |
            DdsStatusKind::kLivelinessLost |
            DdsStatusKind::kLivelinessChanged |
            DdsStatusKind::kPublicationMatched |
            DdsStatusKind::kSubscriptionMatched;

        EXPECT_TRUE(HasStatus(all, DdsStatusKind::kInconsistentTopic));
        EXPECT_TRUE(HasStatus(all, DdsStatusKind::kSubscriptionMatched));
    }

    TEST(DdsStatusConditionTest, DeadlineMissedStatusDefaults)
    {
        DdsDeadlineMissedStatus status;
        EXPECT_EQ(status.TotalCount, 0U);
        EXPECT_EQ(status.TotalCountChange, 0);
    }

    TEST(DdsStatusConditionTest, SampleLostStatusDefaults)
    {
        DdsSampleLostStatus status;
        EXPECT_EQ(status.TotalCount, 0U);
        EXPECT_EQ(status.TotalCountChange, 0);
    }

    TEST(DdsStatusConditionTest, SampleRejectedStatusDefaults)
    {
        DdsSampleRejectedStatus status;
        EXPECT_EQ(status.TotalCount, 0U);
        EXPECT_EQ(status.LastReason,
                  DdsSampleRejectedReason::kNotRejected);
    }

    TEST(DdsStatusConditionTest, LivelinessChangedStatusDefaults)
    {
        DdsLivelinessChangedStatus status;
        EXPECT_EQ(status.AliveCount, 0U);
        EXPECT_EQ(status.NotAliveCount, 0U);
    }

    TEST(DdsStatusConditionTest, MatchedStatusDefaults)
    {
        DdsMatchedStatus status;
        EXPECT_EQ(status.TotalCount, 0U);
        EXPECT_EQ(status.CurrentCount, 0U);
    }

    TEST(DdsStatusConditionTest, IncompatibleQosStatusDefaults)
    {
        DdsIncompatibleQosStatus status;
        EXPECT_EQ(status.TotalCount, 0U);
        EXPECT_EQ(status.LastPolicyId, 0U);
    }

    TEST(DdsStatusConditionTest, ListenerDefault)
    {
        DdsStatusListener listener;
        EXPECT_FALSE(static_cast<bool>(listener.OnSampleLost));
        EXPECT_FALSE(static_cast<bool>(listener.OnSampleRejected));
        EXPECT_FALSE(static_cast<bool>(listener.OnLivelinessChanged));
    }

    TEST(DdsStatusConditionTest, ListenerCallbackAssignment)
    {
        DdsStatusListener listener;
        bool called = false;
        listener.OnSampleLost = [&](const DdsSampleLostStatus &)
        {
            called = true;
        };
        EXPECT_TRUE(static_cast<bool>(listener.OnSampleLost));
        DdsSampleLostStatus status;
        listener.OnSampleLost(status);
        EXPECT_TRUE(called);
    }

    // ── DDS Content Filter Tests ──────────────────────────────

    TEST(DdsContentFilterTest, DefaultDisabled)
    {
        DdsContentFilter filter;
        EXPECT_FALSE(filter.Enabled);
        EXPECT_TRUE(filter.Expression.empty());
        EXPECT_TRUE(filter.Parameters.empty());
    }

    TEST(DdsContentFilterTest, ValidDisabled)
    {
        DdsContentFilter filter;
        EXPECT_TRUE(ValidateContentFilter(filter));
    }

    TEST(DdsContentFilterTest, ValidEnabled)
    {
        DdsContentFilter filter;
        filter.Enabled = true;
        filter.Expression = "speed > 60";
        EXPECT_TRUE(ValidateContentFilter(filter));
    }

    TEST(DdsContentFilterTest, InvalidEmptyExpression)
    {
        DdsContentFilter filter;
        filter.Enabled = true;
        EXPECT_FALSE(ValidateContentFilter(filter));
    }

    TEST(DdsContentFilterTest, InvalidTooManyParams)
    {
        DdsContentFilter filter;
        filter.Enabled = true;
        filter.Expression = "x > %0";
        filter.Parameters.resize(101);
        EXPECT_FALSE(ValidateContentFilter(filter));
    }

    TEST(DdsContentFilterTest, WithParameters)
    {
        DdsContentFilter filter;
        filter.Enabled = true;
        filter.Expression = "speed > %0 AND speed < %1";
        filter.Parameters = {"60", "120"};
        filter.Kind = FilterExpressionKind::kSql;

        EXPECT_TRUE(ValidateContentFilter(filter));
        EXPECT_EQ(filter.Parameters.size(), 2U);
    }

    TEST(DdsContentFilterTest, CustomKind)
    {
        DdsContentFilter filter;
        filter.Kind = FilterExpressionKind::kCustom;
        filter.Enabled = true;
        filter.Expression = "custom_filter()";
        EXPECT_TRUE(ValidateContentFilter(filter));
    }

    // ── DDS Extended QoS Tests ────────────────────────────────

    TEST(DdsExtendedQosTest, OwnershipDefaults)
    {
        DdsExtendedQos qos;
        EXPECT_EQ(qos.Ownership, DdsOwnershipKind::kShared);
        EXPECT_EQ(qos.OwnershipStrength, 0U);
    }

    TEST(DdsExtendedQosTest, ExclusiveOwnership)
    {
        DdsExtendedQos qos;
        qos.Ownership = DdsOwnershipKind::kExclusive;
        qos.OwnershipStrength = 10U;
        EXPECT_EQ(qos.Ownership, DdsOwnershipKind::kExclusive);
        EXPECT_EQ(qos.OwnershipStrength, 10U);
    }

    TEST(DdsExtendedQosTest, PartitionConfig)
    {
        DdsPartitionConfig partCfg;
        partCfg.Partitions = {"partition_a", "partition_b"};
        EXPECT_EQ(partCfg.Partitions.size(), 2U);
    }

    TEST(DdsExtendedQosTest, TransportPriority)
    {
        DdsExtendedQos qos;
        qos.TransportPriority = 5U;
        EXPECT_EQ(qos.TransportPriority, 5U);
    }

    TEST(DdsExtendedQosTest, ResourceLimitsDefaults)
    {
        DdsExtendedQos qos;
        EXPECT_EQ(qos.MaxSamples, -1);
        EXPECT_EQ(qos.MaxInstances, -1);
        EXPECT_EQ(qos.MaxSamplesPerInstance, -1);
    }

    TEST(DdsExtendedQosTest, TimeBasedFilter)
    {
        DdsExtendedQos qos;
        qos.TimeBasedFilterSeparation = std::chrono::milliseconds{100};
        EXPECT_EQ(qos.TimeBasedFilterSeparation.count(), 100);
    }

    TEST(DdsExtendedQosTest, LatencyBudget)
    {
        DdsExtendedQos qos;
        qos.LatencyBudget = std::chrono::milliseconds{50};
        EXPECT_EQ(qos.LatencyBudget.count(), 50);
    }

    // ── DDS QoS Profile Tests (augmenting existing) ───────────

    TEST(DdsQosProfileTest, DomainIdField)
    {
        DdsQosProfile profile;
        EXPECT_EQ(profile.DomainId, 0U);
        profile.DomainId = 42U;
        EXPECT_EQ(profile.DomainId, 42U);
    }

    TEST(DdsQosProfileTest, DeadlinePeriodConfig)
    {
        DdsQosProfile profile;
        profile.DeadlinePeriod = std::chrono::milliseconds{500};
        EXPECT_EQ(profile.DeadlinePeriod.count(), 500);
    }

    TEST(DdsQosProfileTest, LivelinessConfig)
    {
        DdsQosProfile profile;
        profile.Liveliness = DdsLivelinessKind::kManualByTopic;
        profile.LivelinessLeaseDuration = std::chrono::milliseconds{2000};
        EXPECT_EQ(profile.Liveliness, DdsLivelinessKind::kManualByTopic);
        EXPECT_EQ(profile.LivelinessLeaseDuration.count(), 2000);
    }

    TEST(DdsQosProfileTest, ReliabilityTimeout)
    {
        DdsQosProfile profile;
        profile.ReliabilityTimeout = std::chrono::milliseconds{3000};
        EXPECT_EQ(profile.ReliabilityTimeout.count(), 3000);
    }

    TEST(DdsQosProfileTest, KeepAllHistory)
    {
        DdsQosProfile profile;
        profile.History = DdsHistoryKind::kKeepAll;
        EXPECT_EQ(profile.History, DdsHistoryKind::kKeepAll);
    }

    TEST(DdsQosProfileTest, SampleRejectedReasonValues)
    {
        EXPECT_EQ(static_cast<uint8_t>(DdsSampleRejectedReason::kNotRejected), 0U);
        EXPECT_EQ(static_cast<uint8_t>(DdsSampleRejectedReason::kRejectedByInstancesLimit), 1U);
        EXPECT_EQ(static_cast<uint8_t>(DdsSampleRejectedReason::kRejectedBySamplesLimit), 2U);
        EXPECT_EQ(static_cast<uint8_t>(DdsSampleRejectedReason::kRejectedBySamplesPerInstanceLimit), 3U);
    }
}
