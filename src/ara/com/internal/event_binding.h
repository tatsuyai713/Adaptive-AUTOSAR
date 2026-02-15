/// @file src/ara/com/internal/event_binding.h
/// @brief Declarations for event binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_EVENT_BINDING_H
#define ARA_COM_INTERNAL_EVENT_BINDING_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "../../core/result.h"
#include "../types.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief Configuration identifying a specific event within a service
            struct EventBindingConfig
            {
                std::uint16_t ServiceId;
                std::uint16_t InstanceId;
                std::uint16_t EventId;
                std::uint16_t EventGroupId;
                std::uint8_t MajorVersion{1U};
            };

            /// @brief Abstract proxy-side event binding.
            ///        Implementations exist for vsomeip, CycloneDDS, iceoryx.
            ///        Handles subscribe/unsubscribe, message reception, and sample queueing.
            class ProxyEventBinding
            {
            public:
                /// @brief Callback type for raw deserialized payload delivery
                using RawReceiveHandler =
                    std::function<void(const std::uint8_t *, std::size_t)>;

                virtual ~ProxyEventBinding() noexcept = default;

                /// @brief Subscribe to the event
                /// @param maxSampleCount Maximum number of samples to buffer
                virtual core::Result<void> Subscribe(
                    std::size_t maxSampleCount) = 0;

                /// @brief Unsubscribe from the event
                virtual void Unsubscribe() = 0;

                /// @brief Get current subscription state
                virtual SubscriptionState GetSubscriptionState() const noexcept = 0;

                /// @brief Retrieve buffered samples, calling handler for each
                /// @param handler Callback receiving raw bytes for each sample
                /// @param maxNumberOfSamples Maximum number of samples to consume
                /// @returns Number of samples consumed, or error
                virtual core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) = 0;

                /// @brief Set callback invoked when new data arrives (no-arg form per AP spec)
                virtual void SetReceiveHandler(
                    std::function<void()> handler) = 0;

                /// @brief Remove receive handler
                virtual void UnsetReceiveHandler() = 0;

                /// @brief Number of free sample slots available
                virtual std::size_t GetFreeSampleCount() const noexcept = 0;

                /// @brief Set callback invoked when subscription state changes
                virtual void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) = 0;

                /// @brief Remove subscription state change handler
                virtual void UnsetSubscriptionStateChangeHandler() = 0;
            };

            /// @brief Abstract skeleton-side event binding.
            ///        Handles offering events and publishing data to subscribers.
            class SkeletonEventBinding
            {
            public:
                virtual ~SkeletonEventBinding() noexcept = default;

                /// @brief Start offering this event
                virtual core::Result<void> Offer() = 0;

                /// @brief Stop offering this event
                virtual void StopOffer() = 0;

                /// @brief Send serialized payload to all subscribers
                /// @param payload Serialized byte vector
                virtual core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) = 0;

                /// @brief Allocate a buffer for zero-copy send
                /// @param size Desired buffer size in bytes
                /// @returns Pointer to allocated buffer, or error
                virtual core::Result<void *> Allocate(
                    std::size_t size) = 0;

                /// @brief Publish a previously allocated buffer (zero-copy path)
                /// @param data Pointer obtained from Allocate()
                /// @param size Size of the data
                virtual core::Result<void> SendAllocated(
                    void *data, std::size_t size) = 0;
            };
        }
    }
}

#endif
