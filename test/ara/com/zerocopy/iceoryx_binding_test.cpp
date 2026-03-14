#include <gtest/gtest.h>
#include "../../../../src/ara/com/internal/event_binding.h"
#include "../../../../src/ara/com/internal/method_binding.h"
#include "../../../../src/ara/com/internal/queue_overflow_policy.h"
#include "../../../../src/ara/com/zerocopy/zero_copy.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── QueueOverflowPolicy enum tests ────────────────────────────

            TEST(QueueOverflowPolicyTest, DropOldestIsDefault)
            {
                EXPECT_EQ(
                    static_cast<uint8_t>(QueueOverflowPolicy::kDropOldest), 0U);
            }

            TEST(QueueOverflowPolicyTest, RejectNewValue)
            {
                EXPECT_EQ(
                    static_cast<uint8_t>(QueueOverflowPolicy::kRejectNew), 1U);
            }

            TEST(QueueOverflowPolicyTest, EnumDistinct)
            {
                EXPECT_NE(
                    QueueOverflowPolicy::kDropOldest,
                    QueueOverflowPolicy::kRejectNew);
            }

            // ── EventBindingConfig tests ──────────────────────────────────

            TEST(EventBindingConfigTest, DefaultMajorVersion)
            {
                EventBindingConfig config;
                config.ServiceId = 0x1234;
                config.InstanceId = 0x0001;
                config.EventId = 0x8001;
                config.EventGroupId = 0x0001;
                EXPECT_EQ(config.MajorVersion, 1U);
            }

            TEST(EventBindingConfigTest, CustomMajorVersion)
            {
                EventBindingConfig config;
                config.MajorVersion = 3U;
                EXPECT_EQ(config.MajorVersion, 3U);
            }

            // ── MethodBindingConfig tests ─────────────────────────────────

            TEST(MethodBindingConfigTest, DefaultValues)
            {
                MethodBindingConfig config;
                EXPECT_EQ(config.ServiceId, 0U);
                EXPECT_EQ(config.InstanceId, 0U);
                EXPECT_EQ(config.MethodId, 0U);
            }

            TEST(MethodBindingConfigTest, CustomValues)
            {
                MethodBindingConfig config;
                config.ServiceId = 0xABCD;
                config.InstanceId = 0x1234;
                config.MethodId = 0x0001;
                EXPECT_EQ(config.ServiceId, 0xABCD);
                EXPECT_EQ(config.InstanceId, 0x1234);
                EXPECT_EQ(config.MethodId, 0x0001);
            }

            // ── ChannelDescriptor struct tests ────────────────────────

            TEST(ChannelDescriptorTest, DefaultEmpty)
            {
                ara::com::zerocopy::ChannelDescriptor desc;
                EXPECT_TRUE(desc.Service.empty());
                EXPECT_TRUE(desc.Instance.empty());
                EXPECT_TRUE(desc.Event.empty());
            }

            TEST(ChannelDescriptorTest, AssignFields)
            {
                ara::com::zerocopy::ChannelDescriptor desc;
                desc.Service = "MyService";
                desc.Instance = "Instance01";
                desc.Event = "SpeedEvent";
                EXPECT_EQ(desc.Service, "MyService");
                EXPECT_EQ(desc.Instance, "Instance01");
                EXPECT_EQ(desc.Event, "SpeedEvent");
            }

            TEST(ChannelDescriptorTest, CopySemantics)
            {
                ara::com::zerocopy::ChannelDescriptor orig;
                orig.Service = "Svc";
                orig.Instance = "Inst";
                orig.Event = "Evt";

                ara::com::zerocopy::ChannelDescriptor copy = orig;
                EXPECT_EQ(copy.Service, orig.Service);
                EXPECT_EQ(copy.Instance, orig.Instance);
                EXPECT_EQ(copy.Event, orig.Event);
            }
        }
    }
}
