/// @file src/ara/com/dds/dds_content_filter.h
/// @brief DDS Content-Filtered Topic support.
/// @details Per OMG DDS specification, a ContentFilteredTopic allows
///          a subscriber to receive only samples that match a SQL-like
///          filter expression.  This header defines the configuration
///          types that can be attached to an EventBindingConfig to
///          enable content-based filtering in the DDS transport layer.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_CONTENT_FILTER_H
#define ARA_COM_DDS_CONTENT_FILTER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief Content filter expression types.
            enum class FilterExpressionKind : std::uint8_t
            {
                /// @brief SQL-like WHERE clause (CycloneDDS default).
                kSql = 0U,
                /// @brief Custom expression (vendor-specific).
                kCustom = 1U
            };

            /// @brief Configuration for a DDS content-filtered topic.
            struct DdsContentFilter
            {
                /// @brief Filter expression type.
                FilterExpressionKind Kind{FilterExpressionKind::kSql};

                /// @brief SQL filter expression string (e.g. "speed > 60").
                std::string Expression;

                /// @brief Positional parameters substituted into the expression
                ///        (e.g., %0, %1 in the expression).
                std::vector<std::string> Parameters;

                /// @brief Whether filtering is enabled.
                bool Enabled{false};
            };

            /// @brief Validate a content filter configuration.
            /// @returns true if the filter is well-formed (non-empty expression
            ///          when enabled; not too many parameters).
            inline bool ValidateContentFilter(
                const DdsContentFilter &filter) noexcept
            {
                if (!filter.Enabled)
                {
                    return true;
                }
                if (filter.Expression.empty())
                {
                    return false;
                }
                // DDS spec allows up to 100 parameters
                if (filter.Parameters.size() > 100U)
                {
                    return false;
                }
                return true;
            }

            /// @brief Extended QoS policies beyond the basic reliable/best-effort.
            enum class DdsOwnershipKind : std::uint8_t
            {
                kShared = 0U,
                kExclusive = 1U
            };

            /// @brief DDS Partition configuration.
            struct DdsPartitionConfig
            {
                /// @brief Partition names (empty = default partition).
                std::vector<std::string> Partitions;
            };

            /// @brief Extended QoS configuration beyond DdsQosProfile.
            struct DdsExtendedQos
            {
                /// @brief Ownership kind.
                DdsOwnershipKind Ownership{DdsOwnershipKind::kShared};

                /// @brief Ownership strength (only relevant for kExclusive).
                std::uint32_t OwnershipStrength{0U};

                /// @brief Partition configuration.
                DdsPartitionConfig Partition;

                /// @brief Transport priority hint (0 = default).
                std::uint32_t TransportPriority{0U};

                /// @brief Time-based filter minimum separation.
                std::chrono::milliseconds TimeBasedFilterSeparation{0};

                /// @brief Latency budget hint.
                std::chrono::milliseconds LatencyBudget{0};

                /// @brief Resource limits: max samples.
                std::int32_t MaxSamples{-1};

                /// @brief Resource limits: max instances.
                std::int32_t MaxInstances{-1};

                /// @brief Resource limits: max samples per instance.
                std::int32_t MaxSamplesPerInstance{-1};
            };
        }
    }
}

#endif
