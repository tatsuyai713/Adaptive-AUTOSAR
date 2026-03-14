#include <gtest/gtest.h>
#include "../../../../src/ara/com/internal/event_binding.h"
#include "../../../../src/ara/com/internal/method_binding.h"
#include "../../../../src/ara/com/dds/dds_qos_config.h"
#include "../../../../src/ara/com/internal/dds_event_binding.h"
#include "../../../../src/ara/com/internal/dds_method_binding.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── EventBindingConfig with DDS domain ID ─────────────────────

            TEST(DdsBindingConfigTest, DefaultDdsDomainIdIsZero)
            {
                EventBindingConfig config;
                config.ServiceId = 0x1234;
                config.InstanceId = 0x0001;
                config.EventId = 0x8001;
                config.EventGroupId = 0x0001;
                EXPECT_EQ(config.DdsDomainId, 0U);
            }

            TEST(DdsBindingConfigTest, CustomDdsDomainId)
            {
                EventBindingConfig config;
                config.ServiceId = 0x1234;
                config.InstanceId = 0x0001;
                config.EventId = 0x8001;
                config.EventGroupId = 0x0001;
                config.DdsDomainId = 42U;
                EXPECT_EQ(config.DdsDomainId, 42U);
            }

            TEST(DdsMethodBindingConfigTest, DefaultDdsDomainIdIsZero)
            {
                MethodBindingConfig config;
                config.ServiceId = 0x1234;
                config.InstanceId = 0x0001;
                config.MethodId = 0x0001;
                EXPECT_EQ(config.DdsDomainId, 0U);
            }

            TEST(DdsMethodBindingConfigTest, CustomDdsDomainId)
            {
                MethodBindingConfig config;
                config.DdsDomainId = 100U;
                EXPECT_EQ(config.DdsDomainId, 100U);
            }

            // ── DDS topic name generation ─────────────────────────────────

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)

            TEST(DdsEventTopicNameTest, TopicNameFromConfig)
            {
                EventBindingConfig config;
                config.ServiceId = 0x0001;
                config.InstanceId = 0x0002;
                config.EventId = 0x8001;
                config.EventGroupId = 0x0001;

                auto topicName =
                    DdsProxyEventBinding::makeTopicName(config);

                EXPECT_FALSE(topicName.empty());
                EXPECT_NE(topicName.find("0001"), std::string::npos);
            }

            TEST(DdsMethodTopicNameTest, RequestTopicName)
            {
                MethodBindingConfig config;
                config.ServiceId = 0x0010;
                config.InstanceId = 0x0001;
                config.MethodId = 0x0001;

                auto topicName =
                    DdsProxyMethodBinding::makeRequestTopicName(config);

                EXPECT_FALSE(topicName.empty());
                EXPECT_NE(topicName.find("req"), std::string::npos);
            }

            TEST(DdsMethodTopicNameTest, ReplyTopicName)
            {
                MethodBindingConfig config;
                config.ServiceId = 0x0010;
                config.InstanceId = 0x0001;
                config.MethodId = 0x0001;

                auto topicName =
                    DdsProxyMethodBinding::makeReplyTopicName(config);

                EXPECT_FALSE(topicName.empty());
                EXPECT_NE(topicName.find("rep"), std::string::npos);
            }

#endif // ARA_COM_USE_CYCLONEDDS
        }

        // ── DDS QoS configuration tests ───────────────────────────────

        namespace dds
        {
            TEST(DdsQosConfigTest, DefaultReliableQosProfile)
            {
                auto qos = DefaultReliableQos();
                EXPECT_EQ(qos.Reliability, DdsReliability::kReliable);
                EXPECT_EQ(qos.History, DdsHistoryKind::kKeepLast);
                EXPECT_EQ(qos.HistoryDepth, 16U);
                EXPECT_EQ(qos.DeadlinePeriod.count(), 0);
                EXPECT_EQ(qos.Liveliness, DdsLivelinessKind::kAutomatic);
                EXPECT_EQ(qos.LivelinessLeaseDuration.count(), 0);
                EXPECT_EQ(qos.DomainId, 0U);
            }

            TEST(DdsQosConfigTest, BestEffortQosProfile)
            {
                auto qos = BestEffortQos();
                EXPECT_EQ(qos.Reliability, DdsReliability::kBestEffort);
                EXPECT_EQ(qos.History, DdsHistoryKind::kKeepLast);
                EXPECT_EQ(qos.HistoryDepth, 1U);
            }

            TEST(DdsQosConfigTest, CustomQosProfileDeadline)
            {
                DdsQosProfile qos;
                qos.DeadlinePeriod = std::chrono::milliseconds{500};
                EXPECT_EQ(qos.DeadlinePeriod.count(), 500);
            }

            TEST(DdsQosConfigTest, CustomQosProfileLiveliness)
            {
                DdsQosProfile qos;
                qos.Liveliness = DdsLivelinessKind::kManualByTopic;
                qos.LivelinessLeaseDuration = std::chrono::milliseconds{1000};
                EXPECT_EQ(qos.Liveliness, DdsLivelinessKind::kManualByTopic);
                EXPECT_EQ(qos.LivelinessLeaseDuration.count(), 1000);
            }

            TEST(DdsQosConfigTest, QosProfileDomainIdOverride)
            {
                DdsQosProfile qos;
                qos.DomainId = 55U;
                EXPECT_EQ(qos.DomainId, 55U);
            }

            TEST(DdsQosConfigTest, QosProfileHistoryKeepAll)
            {
                DdsQosProfile qos;
                qos.History = DdsHistoryKind::kKeepAll;
                qos.HistoryDepth = 0U;
                EXPECT_EQ(qos.History, DdsHistoryKind::kKeepAll);
            }

            TEST(DdsQosConfigTest, ReliabilityEnumValues)
            {
                EXPECT_EQ(static_cast<uint8_t>(DdsReliability::kBestEffort), 0U);
                EXPECT_EQ(static_cast<uint8_t>(DdsReliability::kReliable), 1U);
            }

            TEST(DdsQosConfigTest, LivelinessEnumValues)
            {
                EXPECT_EQ(
                    static_cast<uint8_t>(DdsLivelinessKind::kAutomatic), 0U);
                EXPECT_EQ(
                    static_cast<uint8_t>(DdsLivelinessKind::kManualByParticipant), 1U);
                EXPECT_EQ(
                    static_cast<uint8_t>(DdsLivelinessKind::kManualByTopic), 2U);
            }
        }
    }
}
