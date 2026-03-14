/// @file src/ara/com/someip/someip_tp.cpp
/// @brief SOME/IP-TP segmentation and reassembly implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include "./someip_tp.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            std::vector<TpSegment> SegmentPayload(
                const std::vector<uint8_t> &payload,
                std::size_t maxSegmentPayload)
            {
                // Per PRS_SOMEIP_00724 the segment payload size must be
                // a multiple of 16 (except possibly the last segment).
                if (maxSegmentPayload == 0U || (maxSegmentPayload % 16U) != 0U)
                {
                    throw std::invalid_argument(
                        "maxSegmentPayload must be a positive multiple of 16");
                }

                std::vector<TpSegment> segments;

                if (payload.empty())
                {
                    // Edge case: empty payload → single segment with no data
                    TpSegment seg;
                    seg.Offset = 0U;
                    seg.MoreSegments = false;
                    segments.push_back(std::move(seg));
                    return segments;
                }

                std::size_t offset = 0U;
                while (offset < payload.size())
                {
                    const std::size_t remaining = payload.size() - offset;
                    const std::size_t chunkSize =
                        std::min(maxSegmentPayload, remaining);

                    TpSegment seg;
                    seg.Offset = static_cast<uint32_t>(offset);
                    seg.MoreSegments = (offset + chunkSize) < payload.size();
                    seg.Payload.assign(
                        payload.begin() + offset,
                        payload.begin() + offset + chunkSize);

                    segments.push_back(std::move(seg));
                    offset += chunkSize;
                }

                return segments;
            }

            TpReassembler::TpReassembler(std::chrono::seconds timeout) noexcept
                : mTimeout{timeout}
            {
            }

            void TpReassembler::AddSegment(
                uint32_t offset,
                bool moreSegments,
                const std::vector<uint8_t> &segmentPayload)
            {
                if (!mStarted)
                {
                    mFirstSegmentTime = std::chrono::steady_clock::now();
                    mStarted = true;
                }

                mSegments[offset] = segmentPayload;

                if (!moreSegments)
                {
                    mLastSegmentReceived = true;
                    mExpectedSize =
                        static_cast<std::size_t>(offset) + segmentPayload.size();
                }
            }

            bool TpReassembler::IsComplete() const noexcept
            {
                if (!mLastSegmentReceived)
                {
                    return false;
                }

                // Verify contiguous coverage from 0 to mExpectedSize
                std::size_t covered = 0U;
                for (const auto &entry : mSegments)
                {
                    if (static_cast<std::size_t>(entry.first) != covered)
                    {
                        return false; // gap detected
                    }
                    covered += entry.second.size();
                }

                return covered == mExpectedSize;
            }

            std::vector<uint8_t> TpReassembler::Reassemble() const
            {
                if (!IsComplete())
                {
                    throw std::logic_error(
                        "TpReassembler::Reassemble() called before all "
                        "segments are available");
                }

                std::vector<uint8_t> result;
                result.reserve(mExpectedSize);

                for (const auto &entry : mSegments)
                {
                    result.insert(
                        result.end(),
                        entry.second.begin(),
                        entry.second.end());
                }

                return result;
            }

            void TpReassembler::Reset() noexcept
            {
                mSegments.clear();
                mLastSegmentReceived = false;
                mExpectedSize = 0U;
                mStarted = false;
            }

            bool TpReassembler::IsTimedOut() const noexcept
            {
                if (!mStarted)
                {
                    return false;
                }
                auto elapsed = std::chrono::steady_clock::now() - mFirstSegmentTime;
                return elapsed > mTimeout;
            }

            std::size_t TpReassembler::SegmentCount() const noexcept
            {
                return mSegments.size();
            }
        }
    }
}
