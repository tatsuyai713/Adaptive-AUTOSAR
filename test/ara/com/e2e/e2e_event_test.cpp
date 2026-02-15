#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "../../../../src/ara/com/e2e/e2e_event.h"
#include "../../../../src/ara/com/e2e/profile11.h"
#include "../../../../src/ara/com/event.h"
#include "../../../../src/ara/com/serialization.h"
#include "../../../../src/ara/com/sample_ptr.h"
#include "../mock_event_binding.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            // Profile11 E2E header size: 1 byte CRC + 1 byte counter/dataID
            static const std::size_t cProfile11HeaderSize{2U};

            class E2ESkeletonEventTest : public ::testing::Test
            {
            protected:
                Profile11 mProfile;
                test::MockSkeletonEventBinding *mMockBinding{nullptr};

                SkeletonEvent<std::uint32_t> CreateE2ESkeletonEvent()
                {
                    auto mock =
                        std::unique_ptr<test::MockSkeletonEventBinding>(
                            new test::MockSkeletonEventBinding());
                    mMockBinding = mock.get();

                    auto e2eBinding =
                        std::unique_ptr<internal::SkeletonEventBinding>(
                            new E2ESkeletonEventBindingDecorator(
                                std::move(mock), mProfile));

                    return SkeletonEvent<std::uint32_t>(std::move(e2eBinding));
                }
            };

            TEST_F(E2ESkeletonEventTest, SendAppliesE2EProtection)
            {
                auto event = CreateE2ESkeletonEvent();
                event.Offer();

                const std::uint32_t value = 0x12345678U;
                event.Send(value);

                ASSERT_EQ(mMockBinding->SentPayloads.size(), 1U);
                const auto &protectedPayload = mMockBinding->SentPayloads[0];

                // Protected payload should be larger than the raw serialized
                // data (4 bytes) by the E2E header (2 bytes)
                EXPECT_EQ(
                    protectedPayload.size(),
                    sizeof(std::uint32_t) + cProfile11HeaderSize);
            }

            TEST_F(E2ESkeletonEventTest, ProtectedPayloadPassesCrcCheck)
            {
                auto event = CreateE2ESkeletonEvent();
                event.Offer();

                const std::uint32_t value = 42U;
                event.Send(value);

                ASSERT_EQ(mMockBinding->SentPayloads.size(), 1U);
                const auto &protectedPayload = mMockBinding->SentPayloads[0];

                // A separate Profile11 checker should accept the protected data
                Profile11 checker;
                CheckStatusType status = checker.Check(protectedPayload);
                // First check on a fresh profile may be kOk or kRepeated
                // depending on counter initialization; just verify it's not
                // kWrongCrc and not kNoNewData
                EXPECT_NE(status, CheckStatusType::kWrongCrc);
                EXPECT_NE(status, CheckStatusType::kNoNewData);
            }

            TEST_F(E2ESkeletonEventTest, MultiplesendsIncrementCounter)
            {
                auto event = CreateE2ESkeletonEvent();
                event.Offer();

                event.Send(1U);
                event.Send(2U);

                ASSERT_EQ(mMockBinding->SentPayloads.size(), 2U);

                // Profile11 stores the sequence counter in the low nibble.
                const std::uint8_t counter1 =
                    static_cast<std::uint8_t>(
                        mMockBinding->SentPayloads[0][1] & 0x0FU);
                const std::uint8_t counter2 =
                    static_cast<std::uint8_t>(
                        mMockBinding->SentPayloads[1][1] & 0x0FU);
                EXPECT_NE(counter1, counter2);
            }

            class E2EProxyEventTest : public ::testing::Test
            {
            protected:
                Profile11 mProtectProfile;
                Profile11 mCheckProfile;
                test::MockProxyEventBinding *mMockBinding{nullptr};

                ProxyEvent<std::uint32_t> CreateE2EProxyEvent()
                {
                    auto mock =
                        std::unique_ptr<test::MockProxyEventBinding>(
                            new test::MockProxyEventBinding());
                    mMockBinding = mock.get();

                    auto e2eBinding =
                        std::unique_ptr<internal::ProxyEventBinding>(
                            new E2EProxyEventBindingDecorator(
                                std::move(mock),
                                mCheckProfile,
                                cProfile11HeaderSize));

                    return ProxyEvent<std::uint32_t>(std::move(e2eBinding));
                }

                std::vector<std::uint8_t> MakeProtectedPayload(
                    std::uint32_t value)
                {
                    auto serialized = Serializer<std::uint32_t>::Serialize(value);
                    std::vector<std::uint8_t> protectedData;
                    mProtectProfile.TryProtect(serialized, protectedData);
                    return protectedData;
                }
            };

            TEST_F(E2EProxyEventTest, GetNewSamplesWithValidCrc)
            {
                auto event = CreateE2EProxyEvent();
                event.Subscribe(10);

                const std::uint32_t expectedValue = 0xDEADBEEFU;
                auto protectedPayload = MakeProtectedPayload(expectedValue);
                mMockBinding->InjectSample(protectedPayload);

                std::uint32_t receivedValue = 0U;
                auto result = event.GetNewSamples(
                    [&receivedValue](SamplePtr<std::uint32_t> sample)
                    {
                        receivedValue = *sample;
                    });

                ASSERT_TRUE(result.HasValue());
                EXPECT_EQ(result.Value(), 1U);
                EXPECT_EQ(receivedValue, expectedValue);
            }

            TEST_F(E2EProxyEventTest, GetNewSamplesDropsCorruptedCrc)
            {
                auto event = CreateE2EProxyEvent();
                event.Subscribe(10);

                // Create valid protected payload then corrupt the CRC byte
                auto protectedPayload = MakeProtectedPayload(42U);
                protectedPayload[0] ^= 0xFFU; // flip CRC byte

                mMockBinding->InjectSample(protectedPayload);

                std::size_t callbackCount = 0U;
                auto result = event.GetNewSamples(
                    [&callbackCount](SamplePtr<std::uint32_t>)
                    {
                        ++callbackCount;
                    });

                ASSERT_TRUE(result.HasValue());
                // The underlying mock returns 1 (it processed 1 raw sample),
                // but the E2E check silently drops the corrupted sample so
                // the callback is never invoked.
                EXPECT_EQ(callbackCount, 0U);
            }

            class E2ERoundTripTest : public ::testing::Test
            {
            protected:
                Profile11 mSenderProfile;
                Profile11 mReceiverProfile;
            };

            TEST_F(E2ERoundTripTest, FullRoundTrip)
            {
                // Sender side
                auto senderMock =
                    std::unique_ptr<test::MockSkeletonEventBinding>(
                        new test::MockSkeletonEventBinding());
                auto *senderMockPtr = senderMock.get();

                auto senderBinding =
                    std::unique_ptr<internal::SkeletonEventBinding>(
                        new E2ESkeletonEventBindingDecorator(
                            std::move(senderMock), mSenderProfile));

                SkeletonEvent<std::uint32_t> senderEvent(
                    std::move(senderBinding));
                senderEvent.Offer();
                senderEvent.Send(12345U);
                senderEvent.Send(67890U);

                // Snapshot protected payloads before wiring receiver side.
                const auto senderPayloads = senderMockPtr->SentPayloads;
                ASSERT_EQ(senderPayloads.size(), 2U);

                // Receiver side
                auto receiverMock =
                    std::unique_ptr<test::MockProxyEventBinding>(
                        new test::MockProxyEventBinding());
                auto *receiverMockPtr = receiverMock.get();

                auto e2eReceiver =
                    std::unique_ptr<internal::ProxyEventBinding>(
                        new E2EProxyEventBindingDecorator(
                            std::move(receiverMock),
                            mReceiverProfile,
                            cProfile11HeaderSize));

                ProxyEvent<std::uint32_t> receiverEvent(
                    std::move(e2eReceiver));
                receiverEvent.Subscribe(10);

                // Feed protected payloads from sender into receiver
                for (const auto &payload : senderPayloads)
                {
                    receiverMockPtr->InjectSample(payload);
                }

                std::vector<std::uint32_t> receivedValues;
                receiverEvent.GetNewSamples(
                    [&receivedValues](SamplePtr<std::uint32_t> sample)
                    {
                        receivedValues.push_back(*sample);
                    });

                ASSERT_EQ(receivedValues.size(), 2U);
                EXPECT_EQ(receivedValues[0], 12345U);
                EXPECT_EQ(receivedValues[1], 67890U);
            }
        }
    }
}
