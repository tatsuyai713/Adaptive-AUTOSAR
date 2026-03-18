#include <gtest/gtest.h>
#include "../../../src/ara/phm/recovery_action.h"
#include "../../../src/ara/phm/phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        static const core::InstanceSpecifier cSpecifier("Instance0");

        class RecoveryActionTest : public RecoveryAction, public testing::Test
        {
        public:
            RecoveryActionTest() : RecoveryAction(cSpecifier)
            {
            }

            int mRecoveryHandlerCallCount{0};
            TypeOfSupervision mLastSupervision{TypeOfSupervision::DeadlineSupervision};

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override
            {
                ++mRecoveryHandlerCallCount;
                mLastSupervision = supervision;
            }
        };

        TEST_F(RecoveryActionTest, Constructor)
        {
            EXPECT_FALSE(IsOffered());
        }

        TEST_F(RecoveryActionTest, OfferMethod)
        {
            const auto cResult{Offer()};
            EXPECT_TRUE(cResult.HasValue());
            EXPECT_TRUE(IsOffered());
        }

        TEST_F(RecoveryActionTest, StopOfferMethod)
        {
            Offer();
            StopOffer();
            EXPECT_FALSE(IsOffered());
        }

        TEST_F(RecoveryActionTest, ExecuteFailsWhenNotOffered)
        {
            auto _result = Execute();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST_F(RecoveryActionTest, ExecuteSucceedsWhenOffered)
        {
            Offer();
            auto _result = Execute();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(mRecoveryHandlerCallCount, 1);
            EXPECT_EQ(mLastSupervision, TypeOfSupervision::AliveSupervision);
        }

        TEST_F(RecoveryActionTest, ExecuteCallsHandlerMultipleTimes)
        {
            Offer();
            Execute();
            Execute();
            Execute();
            EXPECT_EQ(mRecoveryHandlerCallCount, 3);
        }
    }
}