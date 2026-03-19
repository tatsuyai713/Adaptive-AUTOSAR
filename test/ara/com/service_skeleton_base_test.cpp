#include <gtest/gtest.h>
#include <thread>
#include "../../../src/ara/com/service_skeleton_base.h"
#include "../../../src/ara/com/com_error_domain.h"
#include "../../../src/ara/com/instance_identifier.h"

namespace ara
{
    namespace com
    {
        /// @brief Minimal concrete skeleton for testing base class behavior.
        class TestSkeleton : public ServiceSkeletonBase
        {
        public:
            TestSkeleton(
                core::InstanceSpecifier specifier,
                std::uint16_t serviceId,
                std::uint16_t instanceId,
                MethodCallProcessingMode mode =
                    MethodCallProcessingMode::kEvent)
                : ServiceSkeletonBase(
                      std::move(specifier),
                      serviceId,
                      instanceId,
                      1U, 0U, mode)
            {
            }

            using ServiceSkeletonBase::EnqueuePollCall;
            using ServiceSkeletonBase::GetDispatchMutex;
            using ServiceSkeletonBase::GetInstanceId;
            using ServiceSkeletonBase::GetServiceId;
        };

        TEST(ServiceSkeletonBaseTest, Construction)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            EXPECT_EQ(skel.GetServiceId(), 0x1234);
            EXPECT_EQ(skel.GetInstanceId(), 0x0001);
            EXPECT_FALSE(skel.IsOffered());
        }

        TEST(ServiceSkeletonBaseTest, InstanceSpecifier)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel/instance"},
                0x1234, 0x0001);
            EXPECT_EQ(skel.GetInstanceSpecifier().ToString(),
                      "test/skel/instance");
        }

        TEST(ServiceSkeletonBaseTest, OfferAndStopOffer)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            EXPECT_FALSE(skel.IsOffered());

            auto offerResult = skel.OfferService();
            EXPECT_TRUE(offerResult.HasValue());
            EXPECT_TRUE(skel.IsOffered());

            skel.StopOfferService();
            EXPECT_FALSE(skel.IsOffered());
        }

        TEST(ServiceSkeletonBaseTest, DoubleOfferReturnsError)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            auto r1 = skel.OfferService();
            EXPECT_TRUE(r1.HasValue());

            auto r2 = skel.OfferService();
            EXPECT_FALSE(r2.HasValue());
        }

        TEST(ServiceSkeletonBaseTest, DestructorStopsOffer)
        {
            auto skel = std::make_unique<TestSkeleton>(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            auto r = skel->OfferService();
            EXPECT_TRUE(r.HasValue());
            EXPECT_TRUE(skel->IsOffered());
            skel.reset(); // destructor should call StopOfferService
        }

        TEST(ServiceSkeletonBaseTest, GetServiceVersion)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            auto v = skel.GetServiceVersion();
            EXPECT_EQ(v.MajorVersion, 1U);
            EXPECT_EQ(v.MinorVersion, 0U);
        }

        TEST(ServiceSkeletonBaseTest, GetServiceInstanceId)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            auto id = skel.GetServiceInstanceId();
            InstanceIdentifier expected{0x1234, 0x0001};
            EXPECT_EQ(id, expected);
        }

        TEST(ServiceSkeletonBaseTest, GetMethodCallProcessingMode)
        {
            TestSkeleton eventSkel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kEvent);
            EXPECT_EQ(eventSkel.GetMethodCallProcessingMode(),
                      MethodCallProcessingMode::kEvent);

            TestSkeleton pollSkel(
                core::InstanceSpecifier{"test/poll"},
                0x1234, 0x0002,
                MethodCallProcessingMode::kPoll);
            EXPECT_EQ(pollSkel.GetMethodCallProcessingMode(),
                      MethodCallProcessingMode::kPoll);
        }

        TEST(ServiceSkeletonBaseTest, PollModeProcessNextMethodCall)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kPoll);

            bool called{false};
            skel.EnqueuePollCall([&called]() { called = true; });
            EXPECT_TRUE(skel.HasPendingMethodCalls());

            auto result = skel.ProcessNextMethodCall();
            EXPECT_TRUE(result.HasValue());
            EXPECT_TRUE(called);
            EXPECT_FALSE(skel.HasPendingMethodCalls());
        }

        TEST(ServiceSkeletonBaseTest, PollModeNoPending)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kPoll);
            EXPECT_FALSE(skel.HasPendingMethodCalls());
        }

        TEST(ServiceSkeletonBaseTest, PollModeTimeout)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kPoll);
            auto result = skel.ProcessNextMethodCall(
                std::chrono::milliseconds{10});
            EXPECT_TRUE(result.HasValue());
        }

        TEST(ServiceSkeletonBaseTest, EventModeProcessNextIsNoop)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kEvent);
            auto result = skel.ProcessNextMethodCall();
            EXPECT_TRUE(result.HasValue());
        }

        TEST(ServiceSkeletonBaseTest, DispatchMutexForSingleThread)
        {
            TestSkeleton singleSkel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001,
                MethodCallProcessingMode::kEventSingleThread);
            EXPECT_NE(singleSkel.GetDispatchMutex(), nullptr);

            TestSkeleton eventSkel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0002,
                MethodCallProcessingMode::kEvent);
            EXPECT_EQ(eventSkel.GetDispatchMutex(), nullptr);
        }

        TEST(ServiceSkeletonBaseTest, EventSubscriptionStateHandler)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            auto offer = skel.OfferService();
            EXPECT_TRUE(offer.HasValue());

            auto result = skel.SetEventSubscriptionStateHandler(
                0x01,
                [](std::uint16_t, bool) { return true; });
            EXPECT_TRUE(result.HasValue());

            skel.UnsetEventSubscriptionStateHandler(0x01);
        }

        TEST(ServiceSkeletonBaseTest, SetHandlerNotOfferedReturnsError)
        {
            TestSkeleton skel(
                core::InstanceSpecifier{"test/skel"},
                0x1234, 0x0001);
            // Not offered
            auto result = skel.SetEventSubscriptionStateHandler(
                0x01,
                [](std::uint16_t, bool) { return true; });
            EXPECT_FALSE(result.HasValue());
        }
    }
}
