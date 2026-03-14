/// @file src/ara/com/dds/dds_qos_config.h
/// @brief DDS Quality of Service configuration for ara::com bindings.
/// @details Provides configurable QoS profiles that map from the AUTOSAR
///          ara::com EventQosProfile to CycloneDDS QoS parameters.
///          This allows users to control reliability, history depth,
///          deadline monitoring, and liveliness properties per-event.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_QOS_CONFIG_H
#define ARA_COM_DDS_QOS_CONFIG_H

#include <chrono>
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief DDS reliability kind
            enum class DdsReliability : std::uint8_t
            {
                kBestEffort = 0U,
                kReliable = 1U
            };

            /// @brief DDS history kind
            enum class DdsHistoryKind : std::uint8_t
            {
                kKeepLast = 0U,
                kKeepAll = 1U
            };

            /// @brief DDS liveliness kind
            enum class DdsLivelinessKind : std::uint8_t
            {
                kAutomatic = 0U,
                kManualByParticipant = 1U,
                kManualByTopic = 2U
            };

            /// @brief Complete DDS QoS profile for event bindings.
            ///        Defaults match the current hardcoded reliable/keep-last-16 settings.
            struct DdsQosProfile
            {
                /// @brief Reliability mode
                DdsReliability Reliability{DdsReliability::kReliable};

                /// @brief Reliability max-blocking time
                std::chrono::milliseconds ReliabilityTimeout{5000};

                /// @brief History kind
                DdsHistoryKind History{DdsHistoryKind::kKeepLast};

                /// @brief History depth (for KeepLast)
                std::uint32_t HistoryDepth{16U};

                /// @brief Deadline period (0 = no deadline monitoring)
                std::chrono::milliseconds DeadlinePeriod{0};

                /// @brief Liveliness kind
                DdsLivelinessKind Liveliness{DdsLivelinessKind::kAutomatic};

                /// @brief Liveliness lease duration (0 = infinite)
                std::chrono::milliseconds LivelinessLeaseDuration{0};

                /// @brief DDS domain ID (0 = use binding config default)
                std::uint32_t DomainId{0U};
            };

            /// @brief Default QoS profile matching AUTOSAR reliable semantics
            inline DdsQosProfile DefaultReliableQos() noexcept
            {
                return DdsQosProfile{};
            }

            /// @brief Best-effort QoS profile for high-throughput/low-latency events
            inline DdsQosProfile BestEffortQos() noexcept
            {
                DdsQosProfile profile;
                profile.Reliability = DdsReliability::kBestEffort;
                profile.History = DdsHistoryKind::kKeepLast;
                profile.HistoryDepth = 1U;
                return profile;
            }
        }
    }
}

#endif
