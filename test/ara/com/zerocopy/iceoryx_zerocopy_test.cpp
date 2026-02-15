#include <utility>
#include <gtest/gtest.h>
#include "../../../../src/ara/com/com_error_domain.h"
#include "../../../../src/ara/com/zerocopy/zero_copy.h"

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            namespace
            {
                core::ErrorDomain::CodeType ToCode(ComErrc code)
                {
                    return static_cast<core::ErrorDomain::CodeType>(code);
                }

                core::ErrorDomain::CodeType ExpectedInactiveBindingCode()
                {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                    return ToCode(ComErrc::kNetworkBindingFailure);
#else
                    return ToCode(ComErrc::kCommunicationStackError);
#endif
                }

                core::ErrorDomain::CodeType ExpectedInvalidPublishCode()
                {
#if defined(ARA_COM_USE_ICEORYX) && (ARA_COM_USE_ICEORYX == 1)
                    return ToCode(ComErrc::kIllegalUseOfAllocate);
#else
                    return ToCode(ComErrc::kCommunicationStackError);
#endif
                }
            }

            TEST(IceoryxZeroCopyTest, DefaultLoanedSampleIsInvalid)
            {
                LoanedSample _sample;
                EXPECT_FALSE(_sample.IsValid());
                EXPECT_EQ(nullptr, _sample.Data());
                EXPECT_EQ(0U, _sample.Size());
            }

            TEST(IceoryxZeroCopyTest, DefaultReceivedSampleIsInvalid)
            {
                ReceivedSample _sample;
                EXPECT_FALSE(_sample.IsValid());
                EXPECT_EQ(nullptr, _sample.Data());
                EXPECT_EQ(0U, _sample.Size());
            }

            TEST(IceoryxZeroCopyTest, InvalidChannelKeepsPublisherInactive)
            {
                ZeroCopyPublisher _publisher{{"", "inst_0x0001", "evt_0x0001"}};
                EXPECT_FALSE(_publisher.IsBindingActive());
                EXPECT_FALSE(_publisher.HasSubscribers());
            }

            TEST(IceoryxZeroCopyTest, LoanWithZeroPayloadFailsAsIllegalUse)
            {
                ZeroCopyPublisher _publisher{{"", "", ""}};
                LoanedSample _sample;

                auto _result{_publisher.Loan(0U, _sample)};
                EXPECT_FALSE(_result.HasValue());
                EXPECT_EQ(ToCode(ComErrc::kIllegalUseOfAllocate), _result.Error().Value());
                EXPECT_FALSE(_sample.IsValid());
            }

            TEST(IceoryxZeroCopyTest, LoanWithoutActiveBindingFails)
            {
                ZeroCopyPublisher _publisher{{"", "", ""}};
                LoanedSample _sample;

                auto _result{_publisher.Loan(8U, _sample)};
                EXPECT_FALSE(_result.HasValue());
                EXPECT_EQ(ExpectedInactiveBindingCode(), _result.Error().Value());
                EXPECT_FALSE(_sample.IsValid());
            }

            TEST(IceoryxZeroCopyTest, PublishWithInvalidSampleFails)
            {
                ZeroCopyPublisher _publisher{{"", "", ""}};
                LoanedSample _sample;

                auto _result{_publisher.Publish(std::move(_sample))};
                EXPECT_FALSE(_result.HasValue());
                EXPECT_EQ(ExpectedInvalidPublishCode(), _result.Error().Value());
            }

            TEST(IceoryxZeroCopyTest, SubscriberTryTakeWithoutBindingFails)
            {
                ZeroCopySubscriber _subscriber{{"", "", ""}};
                ReceivedSample _sample;

                auto _result{_subscriber.TryTake(_sample)};
                EXPECT_FALSE(_result.HasValue());
                EXPECT_EQ(ExpectedInactiveBindingCode(), _result.Error().Value());
                EXPECT_FALSE(_sample.IsValid());
            }
        }
    }
}
