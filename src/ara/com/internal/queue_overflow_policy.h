/// @file src/ara/com/internal/queue_overflow_policy.h
/// @brief Queue overflow policy for event sample queues.
/// @details Per AUTOSAR SWS_CM_00710, when the sample queue is full,
///          the oldest sample should be replaced by default, but the
///          application may configure the policy.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_QUEUE_OVERFLOW_POLICY_H
#define ARA_COM_INTERNAL_QUEUE_OVERFLOW_POLICY_H

#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief Policy for handling event sample queue overflow.
            enum class QueueOverflowPolicy : std::uint8_t
            {
                /// @brief Drop the oldest sample to make room for the new one (default).
                kDropOldest = 0U,

                /// @brief Reject the new sample when the queue is full.
                kRejectNew = 1U
            };
        }
    }
}

#endif
