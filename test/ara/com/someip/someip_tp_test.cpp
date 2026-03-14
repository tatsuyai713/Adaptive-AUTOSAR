#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../../../../src/ara/com/someip/someip_tp.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            TEST(TpReassemblerExtendedTest, SegmentCountStartsAtZero)
            {
                TpReassembler reassembler;
                EXPECT_EQ(reassembler.SegmentCount(), 0U);
            }

            TEST(TpReassemblerExtendedTest, SegmentCountIncrementsOnAdd)
            {
                TpReassembler reassembler;
                std::vector<uint8_t> seg1(16, 0xAA);
                std::vector<uint8_t> seg2(16, 0xBB);

                reassembler.AddSegment(0, true, seg1);
                EXPECT_EQ(reassembler.SegmentCount(), 1U);

                reassembler.AddSegment(16, false, seg2);
                EXPECT_EQ(reassembler.SegmentCount(), 2U);
            }

            TEST(TpReassemblerExtendedTest, IsTimedOutReturnsFalseBeforeStart)
            {
                TpReassembler reassembler;
                EXPECT_FALSE(reassembler.IsTimedOut());
            }

            TEST(TpReassemblerExtendedTest, IsTimedOutReturnsFalseImmediately)
            {
                TpReassembler reassembler;
                std::vector<uint8_t> seg(16, 0xAA);
                reassembler.AddSegment(0, true, seg);
                EXPECT_FALSE(reassembler.IsTimedOut());
            }

            TEST(TpReassemblerExtendedTest, CustomTimeoutConstructor)
            {
                TpReassembler reassembler(std::chrono::seconds{10});
                std::vector<uint8_t> seg(16, 0xAA);
                reassembler.AddSegment(0, true, seg);
                EXPECT_FALSE(reassembler.IsTimedOut());
            }

            TEST(TpReassemblerExtendedTest, VeryShortTimeoutExpires)
            {
                // 1ms timeout should expire after a small sleep
                TpReassembler reassembler(std::chrono::seconds{0});
                // Use the TpReassembler with a timeout of 0 seconds
                // which means any elapsed time will trigger it
                std::vector<uint8_t> seg(16, 0xAA);
                reassembler.AddSegment(0, true, seg);

                // Sleep a bit to ensure timeout
                std::this_thread::sleep_for(std::chrono::milliseconds{10});

                EXPECT_TRUE(reassembler.IsTimedOut());
            }

            TEST(TpReassemblerExtendedTest, ResetClearsTimeout)
            {
                TpReassembler reassembler(std::chrono::seconds{0});
                std::vector<uint8_t> seg(16, 0xAA);
                reassembler.AddSegment(0, true, seg);
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
                EXPECT_TRUE(reassembler.IsTimedOut());

                reassembler.Reset();
                EXPECT_FALSE(reassembler.IsTimedOut());
                EXPECT_EQ(reassembler.SegmentCount(), 0U);
            }

            TEST(TpReassemblerExtendedTest, ResetClearsSegmentCount)
            {
                TpReassembler reassembler;
                std::vector<uint8_t> seg(16, 0xAA);
                reassembler.AddSegment(0, true, seg);
                reassembler.AddSegment(16, false, seg);
                EXPECT_EQ(reassembler.SegmentCount(), 2U);

                reassembler.Reset();
                EXPECT_EQ(reassembler.SegmentCount(), 0U);
            }

            TEST(TpReassemblerExtendedTest, CompleteReassemblyWithTimeout)
            {
                TpReassembler reassembler(std::chrono::seconds{60});

                std::vector<uint8_t> seg1(16, 0x11);
                std::vector<uint8_t> seg2(8, 0x22);

                reassembler.AddSegment(0, true, seg1);
                reassembler.AddSegment(16, false, seg2);

                EXPECT_TRUE(reassembler.IsComplete());
                EXPECT_FALSE(reassembler.IsTimedOut());

                auto result = reassembler.Reassemble();
                EXPECT_EQ(result.size(), 24U);
            }

            TEST(TpSegmentationTest, SegmentEmptyPayload)
            {
                std::vector<uint8_t> empty;
                auto segments = SegmentPayload(empty);

                EXPECT_EQ(segments.size(), 1U);
                EXPECT_FALSE(segments[0].MoreSegments);
                EXPECT_EQ(segments[0].Offset, 0U);
                EXPECT_TRUE(segments[0].Payload.empty());
            }

            TEST(TpSegmentationTest, SegmentSmallPayload)
            {
                std::vector<uint8_t> payload(100, 0xAB);
                auto segments = SegmentPayload(payload);

                // 100 bytes < 1392 default segment size → single segment
                EXPECT_EQ(segments.size(), 1U);
                EXPECT_FALSE(segments[0].MoreSegments);
                EXPECT_EQ(segments[0].Offset, 0U);
                EXPECT_EQ(segments[0].Payload.size(), 100U);
            }

            TEST(TpSegmentationTest, SegmentLargePayload)
            {
                // 3000 bytes with 1392 segment size → 3 segments
                std::vector<uint8_t> payload(3000, 0xCD);
                auto segments = SegmentPayload(payload, 1392);

                EXPECT_EQ(segments.size(), 3U);

                // First segment
                EXPECT_EQ(segments[0].Offset, 0U);
                EXPECT_TRUE(segments[0].MoreSegments);
                EXPECT_EQ(segments[0].Payload.size(), 1392U);

                // Second segment
                EXPECT_EQ(segments[1].Offset, 1392U);
                EXPECT_TRUE(segments[1].MoreSegments);
                EXPECT_EQ(segments[1].Payload.size(), 1392U);

                // Third (last) segment
                EXPECT_EQ(segments[2].Offset, 2784U);
                EXPECT_FALSE(segments[2].MoreSegments);
                EXPECT_EQ(segments[2].Payload.size(), 216U);
            }

            TEST(TpSegmentationTest, InvalidSegmentSizeThrows)
            {
                std::vector<uint8_t> payload(100, 0x42);
                EXPECT_THROW(SegmentPayload(payload, 0), std::invalid_argument);
                EXPECT_THROW(SegmentPayload(payload, 15), std::invalid_argument);
                EXPECT_THROW(SegmentPayload(payload, 100), std::invalid_argument);
            }

            TEST(TpSegmentationTest, RoundTripSegmentReassemble)
            {
                // Generate a payload, segment it, then reassemble
                std::vector<uint8_t> original(5000);
                for (std::size_t i = 0; i < original.size(); ++i)
                {
                    original[i] = static_cast<uint8_t>(i & 0xFF);
                }

                auto segments = SegmentPayload(original, 1392);
                EXPECT_GT(segments.size(), 1U);

                TpReassembler reassembler;
                for (const auto &seg : segments)
                {
                    reassembler.AddSegment(
                        seg.Offset, seg.MoreSegments, seg.Payload);
                }

                EXPECT_TRUE(reassembler.IsComplete());

                auto reassembled = reassembler.Reassemble();
                EXPECT_EQ(reassembled, original);
            }
        }
    }
}
