/// @file src/ara/com/zerocopy/zerocopy_config.h
/// @brief Configurable zero-copy transport settings.
/// @details Per AUTOSAR AP SWS_CM, the zero-copy transport layer should
///          be configurable for runtime name, queue capacity, history,
///          and other parameters.  This header provides the configuration
///          types used by the iceoryx backend.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_ZEROCOPY_CONFIG_H
#define ARA_COM_ZEROCOPY_CONFIG_H

#include <chrono>
#include <cstdint>
#include <string>

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            /// @brief Default subscriber queue capacity.
            static constexpr std::uint64_t cDefaultQueueCapacity{256U};

            /// @brief Default publisher history capacity.
            static constexpr std::uint64_t cDefaultHistoryCapacity{0U};

            /// @brief Default runtime name for iceoryx PoshRuntime.
            static const char *const cDefaultRuntimeName =
                "adaptive_autosar_ara_com";

            /// @brief Subscriber-side zero-copy configuration.
            struct SubscriberConfig
            {
                /// @brief iceoryx subscriber queue capacity.
                std::uint64_t QueueCapacity{cDefaultQueueCapacity};

                /// @brief Number of historical samples to request on subscribe.
                std::uint64_t HistoryRequest{0U};

                /// @brief Whether to subscribe on construction or defer.
                bool SubscribeOnCreate{true};

                /// @brief Process-unique iceoryx runtime name for this subscriber.
                std::string RuntimeName{cDefaultRuntimeName};
            };

            /// @brief Publisher-side zero-copy configuration.
            struct PublisherConfig
            {
                /// @brief Number of historical samples retained by the publisher.
                std::uint64_t HistoryCapacity{cDefaultHistoryCapacity};

                /// @brief Whether to offer on construction or defer.
                bool OfferOnCreate{true};

                /// @brief Process-unique iceoryx runtime name for this publisher.
                std::string RuntimeName{cDefaultRuntimeName};
            };

            /// @brief Combined zero-copy transport configuration.
            struct ZeroCopyTransportConfig
            {
                /// @brief Default subscriber settings.
                SubscriberConfig Subscriber;

                /// @brief Default publisher settings.
                PublisherConfig Publisher;

                /// @brief Maximum chunk payload size for loan operations (bytes).
                ///        0 = iceoryx default (unlimited up to shared memory capacity).
                std::uint64_t MaxChunkPayloadSize{0U};

                /// @brief Poll timeout for background receive threads.
                std::chrono::milliseconds PollTimeout{50};
            };

            /// @brief Default transport configuration factory.
            inline ZeroCopyTransportConfig DefaultZeroCopyConfig() noexcept
            {
                return ZeroCopyTransportConfig{};
            }
        }
    }
}

#endif
