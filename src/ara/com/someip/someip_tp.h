/// @file src/ara/com/someip/someip_tp.h
/// @brief SOME/IP-TP (Transport Protocol) segmentation and reassembly.
/// @details Per AUTOSAR PRS_SOMEIP_00720–PRS_SOMEIP_00724, SOME/IP-TP allows
///          transmitting payloads that exceed the transport-layer MTU
///          (typically 1400 bytes for UDP) by splitting them into segments.
///          Each segment carries a 4-byte TP header: offset [31:4] and a
///          more-segments flag [0].

#ifndef ARA_COM_SOMEIP_SOMEIP_TP_H
#define ARA_COM_SOMEIP_SOMEIP_TP_H

#include <cstdint>
#include <map>
#include <vector>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief Default maximum segment payload size (bytes).
            ///        Must be a multiple of 16 per the SOME/IP-TP specification.
            static constexpr std::size_t cDefaultTpSegmentSize{1392U};

            /// @brief SOME/IP-TP segment descriptor produced by the segmenter.
            struct TpSegment
            {
                /// @brief Byte offset of this segment within the reassembled payload.
                uint32_t Offset{0U};

                /// @brief True if more segments follow.
                bool MoreSegments{false};

                /// @brief Segment payload bytes.
                std::vector<uint8_t> Payload;
            };

            /// @brief Split a large payload into SOME/IP-TP segments.
            /// @param payload Full payload to segment
            /// @param maxSegmentPayload Maximum per-segment payload size
            ///        (must be a multiple of 16; default 1392)
            /// @returns Ordered vector of TpSegment descriptors
            std::vector<TpSegment> SegmentPayload(
                const std::vector<uint8_t> &payload,
                std::size_t maxSegmentPayload = cDefaultTpSegmentSize);

            /// @brief Stateful reassembler that collects SOME/IP-TP segments
            ///        and reconstructs the original payload.
            class TpReassembler
            {
            private:
                /// @brief Collected segments keyed by byte offset.
                std::map<uint32_t, std::vector<uint8_t>> mSegments;

                /// @brief True once the final segment (more=false) has been received.
                bool mLastSegmentReceived{false};

                /// @brief Expected total payload size (derived from final segment).
                std::size_t mExpectedSize{0U};

            public:
                TpReassembler() = default;

                /// @brief Feed a segment into the reassembler.
                /// @param offset Byte offset from the TP header
                /// @param moreSegments More-segments flag from the TP header
                /// @param segmentPayload The segment's payload bytes
                void AddSegment(uint32_t offset,
                                bool moreSegments,
                                const std::vector<uint8_t> &segmentPayload);

                /// @brief Check whether all segments have been received
                ///        and the payload can be reassembled.
                /// @returns True when the full payload is available.
                bool IsComplete() const noexcept;

                /// @brief Reassemble the full payload from collected segments.
                /// @pre IsComplete() must return true.
                /// @returns The reassembled payload byte vector.
                std::vector<uint8_t> Reassemble() const;

                /// @brief Reset the reassembler to accept a new message.
                void Reset() noexcept;
            };
        }
    }
}

#endif
