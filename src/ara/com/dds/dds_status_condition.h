/// @file src/ara/com/dds/dds_status_condition.h
/// @brief DDS status condition and listener callback types.
/// @details Per DDS specification, the middleware can report various
///          conditions to the application: deadline missed, liveliness
///          lost/changed, sample lost/rejected, etc.  These types define
///          the status structures and callback signatures for ara::com
///          DDS bindings to report DDS-level conditions to the user.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_STATUS_CONDITION_H
#define ARA_COM_DDS_STATUS_CONDITION_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief DDS status kind flags per OMG DDS specification.
            enum class DdsStatusKind : std::uint32_t
            {
                kNone = 0x0000U,
                kInconsistentTopic = 0x0001U,
                kOfferedDeadlineMissed = 0x0002U,
                kRequestedDeadlineMissed = 0x0004U,
                kOfferedIncompatibleQos = 0x0020U,
                kRequestedIncompatibleQos = 0x0040U,
                kSampleLost = 0x0080U,
                kSampleRejected = 0x0100U,
                kDataOnReaders = 0x0200U,
                kDataAvailable = 0x0400U,
                kLivelinessLost = 0x0800U,
                kLivelinessChanged = 0x1000U,
                kPublicationMatched = 0x2000U,
                kSubscriptionMatched = 0x4000U
            };

            /// @brief Combine DDS status kinds with bitwise OR.
            inline DdsStatusKind operator|(
                DdsStatusKind a, DdsStatusKind b) noexcept
            {
                return static_cast<DdsStatusKind>(
                    static_cast<std::uint32_t>(a) |
                    static_cast<std::uint32_t>(b));
            }

            /// @brief Check if a status kind is set.
            inline bool HasStatus(
                DdsStatusKind flags, DdsStatusKind check) noexcept
            {
                return (static_cast<std::uint32_t>(flags) &
                        static_cast<std::uint32_t>(check)) != 0U;
            }

            /// @brief Sample rejection reason per DDS specification.
            enum class DdsSampleRejectedReason : std::uint8_t
            {
                kNotRejected = 0U,
                kRejectedByInstancesLimit = 1U,
                kRejectedBySamplesLimit = 2U,
                kRejectedBySamplesPerInstanceLimit = 3U
            };

            /// @brief Deadline missed status report.
            struct DdsDeadlineMissedStatus
            {
                std::uint32_t TotalCount{0U};
                std::int32_t TotalCountChange{0};
            };

            /// @brief Sample lost status report.
            struct DdsSampleLostStatus
            {
                std::uint32_t TotalCount{0U};
                std::int32_t TotalCountChange{0};
            };

            /// @brief Sample rejected status report.
            struct DdsSampleRejectedStatus
            {
                std::uint32_t TotalCount{0U};
                std::int32_t TotalCountChange{0};
                DdsSampleRejectedReason LastReason{
                    DdsSampleRejectedReason::kNotRejected};
            };

            /// @brief Liveliness changed status report.
            struct DdsLivelinessChangedStatus
            {
                std::uint32_t AliveCount{0U};
                std::uint32_t NotAliveCount{0U};
                std::int32_t AliveCountChange{0};
                std::int32_t NotAliveCountChange{0};
            };

            /// @brief Subscription/publication match status.
            struct DdsMatchedStatus
            {
                std::uint32_t TotalCount{0U};
                std::int32_t TotalCountChange{0};
                std::uint32_t CurrentCount{0U};
                std::int32_t CurrentCountChange{0};
            };

            /// @brief Incompatible QoS status.
            struct DdsIncompatibleQosStatus
            {
                std::uint32_t TotalCount{0U};
                std::int32_t TotalCountChange{0};
                std::uint32_t LastPolicyId{0U};
            };

            /// @brief Aggregated DDS status listener callbacks.
            struct DdsStatusListener
            {
                std::function<void(const DdsDeadlineMissedStatus &)>
                    OnOfferedDeadlineMissed;
                std::function<void(const DdsDeadlineMissedStatus &)>
                    OnRequestedDeadlineMissed;
                std::function<void(const DdsSampleLostStatus &)>
                    OnSampleLost;
                std::function<void(const DdsSampleRejectedStatus &)>
                    OnSampleRejected;
                std::function<void(const DdsLivelinessChangedStatus &)>
                    OnLivelinessChanged;
                std::function<void(const DdsMatchedStatus &)>
                    OnPublicationMatched;
                std::function<void(const DdsMatchedStatus &)>
                    OnSubscriptionMatched;
                std::function<void(const DdsIncompatibleQosStatus &)>
                    OnOfferedIncompatibleQos;
                std::function<void(const DdsIncompatibleQosStatus &)>
                    OnRequestedIncompatibleQos;
            };
        }
    }
}

#endif
