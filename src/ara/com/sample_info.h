/// @file src/ara/com/sample_info.h
/// @brief Per-sample metadata per AUTOSAR AP SWS_CM_00321.
/// @details Provides metadata associated with each received event sample,
///          including timestamp, source information, and E2E status.

#ifndef ARA_COM_SAMPLE_INFO_H
#define ARA_COM_SAMPLE_INFO_H

#include <chrono>
#include <cstdint>

namespace ara
{
    namespace com
    {
        /// @brief E2E check result for a received sample per SWS_CM_01001.
        enum class E2ESampleStatus : std::uint8_t
        {
            kOk = 0U,            ///< E2E check passed.
            kRepeated = 1U,      ///< Sample counter is repeated.
            kWrongSequence = 2U, ///< Out-of-sequence counter detected.
            kError = 3U,         ///< CRC error on sample data.
            kNotAvailable = 4U,  ///< E2E status not available for this sample.
            kNoCheck = 5U        ///< No E2E protection configured.
        };

        /// @brief Metadata associated with a received event sample per SWS_CM_00321.
        ///        Optionally provided alongside each sample in GetNewSamples().
        struct SampleInfo
        {
            /// @brief Timestamp when the sample was received by the middleware.
            std::chrono::steady_clock::time_point ReceiveTimestamp{};

            /// @brief Source instance identifier (0 = unknown).
            std::uint16_t SourceInstanceId{0U};

            /// @brief E2E protection check result for this sample.
            E2ESampleStatus E2EStatus{E2ESampleStatus::kNoCheck};

            /// @brief Monotonic sequence number assigned by the middleware.
            std::uint64_t SequenceNumber{0U};

            /// @brief Creates a default SampleInfo with current timestamp.
            /// @returns SampleInfo with receive time set to now.
            static SampleInfo Now() noexcept
            {
                SampleInfo info;
                info.ReceiveTimestamp =
                    std::chrono::steady_clock::now();
                return info;
            }
        };
    }
}

#endif
