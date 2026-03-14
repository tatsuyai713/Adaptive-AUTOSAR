/// @file src/ara/com/event.h
/// @brief Declarations for event.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_EVENT_H
#define ARA_COM_EVENT_H

#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include "./types.h"
#include "./qos.h"
#include "./sample_ptr.h"
#include "./serialization.h"
#include "./internal/event_binding.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Proxy-side event wrapper per AUTOSAR AP SWS_CM_00301.
        ///        This is a typed member of a generated proxy class.
        /// @tparam T Event data type
        template <typename T>
        class ProxyEvent
        {
        private:
            std::unique_ptr<internal::ProxyEventBinding> mBinding;

        public:
            /// @brief Construct from a binding implementation
            /// @param binding Proxy event binding instance.
            explicit ProxyEvent(
                std::unique_ptr<internal::ProxyEventBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            ProxyEvent(const ProxyEvent &) = delete;
            ProxyEvent &operator=(const ProxyEvent &) = delete;
            /// @brief Move constructor.
            ProxyEvent(ProxyEvent &&) noexcept = default;
            /// @brief Move assignment.
            /// @returns Reference to `*this`.
            ProxyEvent &operator=(ProxyEvent &&) noexcept = default;

            /// @brief Subscribe to this event
            /// @param maxSampleCount Maximum number of samples to buffer
            void Subscribe(std::size_t maxSampleCount)
            {
                if (mBinding)
                {
                    mBinding->Subscribe(maxSampleCount);
                }
            }

            /// @brief Subscribe with cache update policy (SWS_CM_00701).
            /// @param maxSampleCount Maximum number of samples to buffer.
            /// @param policy Cache update policy for sample management.
            void Subscribe(std::size_t maxSampleCount,
                           EventCacheUpdatePolicy policy)
            {
                // Educational: policy stored but behavior is binding-dependent.
                (void)policy;
                Subscribe(maxSampleCount);
            }

            /// @brief Subscribe with event filter (SWS_CM_00702).
            /// @param maxSampleCount Maximum number of samples to buffer.
            /// @param filter Filter configuration for value/rate filtering.
            void Subscribe(std::size_t maxSampleCount,
                           const FilterConfig &filter)
            {
                // Educational: filter stored but binding-dependent.
                (void)filter;
                Subscribe(maxSampleCount);
            }

            /// @brief Subscribe with QoS profile (SWS_CM_00920).
            /// @param maxSampleCount Maximum number of samples to buffer.
            /// @param qos Quality of Service profile for this subscription.
            void Subscribe(std::size_t maxSampleCount,
                           const EventQosProfile &qos)
            {
                // Educational: QoS stored but binding-dependent.
                (void)qos;
                Subscribe(maxSampleCount);
            }

            /// @brief Unsubscribe from this event
            void Unsubscribe()
            {
                if (mBinding)
                {
                    mBinding->Unsubscribe();
                }
            }

            /// @brief Get current subscription state
            /// @returns Current state of event subscription.
            SubscriptionState GetSubscriptionState() const noexcept
            {
                if (mBinding)
                {
                    return mBinding->GetSubscriptionState();
                }
                return SubscriptionState::kNotSubscribed;
            }

            /// @brief Get new samples, invoking callback for each deserialized sample.
            ///        This is the standard AUTOSAR AP pattern:
            ///        proxy.SomeEvent.GetNewSamples([](SamplePtr<T> sample){ ... });
            /// @tparam F Callable taking SamplePtr<T>
            /// @param f Handler called for each received sample
            /// @param maxNumberOfSamples Maximum samples to retrieve (default: all)
            /// @returns Number of samples processed, or error
            template <typename F>
            core::Result<std::size_t> GetNewSamples(
                F &&f,
                std::size_t maxNumberOfSamples =
                    std::numeric_limits<std::size_t>::max())
            {
                if (!mBinding)
                {
                    return core::Result<std::size_t>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                }

                bool hasDeserializationError{false};

                auto receiveResult = mBinding->GetNewSamples(
                    [&f, &hasDeserializationError](
                        const std::uint8_t *data,
                        std::size_t size)
                    {
                        auto deserResult =
                            Serializer<T>::Deserialize(data, size);
                        if (deserResult.HasValue())
                        {
                            auto sample = std::make_unique<const T>(
                                std::move(deserResult).Value());
                            f(SamplePtr<T>{std::move(sample)});
                        }
                        else if (!hasDeserializationError)
                        {
                            hasDeserializationError = true;
                        }
                    },
                    maxNumberOfSamples);

                if (!receiveResult.HasValue())
                {
                    return receiveResult;
                }

                if (hasDeserializationError)
                {
                    return core::Result<std::size_t>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                return receiveResult;
            }

            /// @brief Set handler called when new data arrives (no-argument form)
            /// @param handler Callback invoked when new event data is available.
            void SetReceiveHandler(EventReceiveHandler handler)
            {
                if (mBinding)
                {
                    mBinding->SetReceiveHandler(std::move(handler));
                }
            }

            /// @brief Set handler called when new data arrives (sized form, SWS_CM_00318).
            /// @param handler Callback with count of new samples available.
            void SetReceiveHandler(SizedEventReceiveHandler handler)
            {
                if (mBinding && handler)
                {
                    // Wrap sized handler into no-arg form for binding compatibility.
                    auto wrappedHandler = handler;
                    mBinding->SetReceiveHandler(
                        [wrappedHandler]()
                        {
                            // Count is reported as 1 per notification.
                            wrappedHandler(1U);
                        });
                }
            }

            /// @brief Remove receive handler
            void UnsetReceiveHandler()
            {
                if (mBinding)
                {
                    mBinding->UnsetReceiveHandler();
                }
            }

            /// @brief Set handler for subscription state changes
            /// @param handler Callback invoked when subscription state changes.
            void SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                if (mBinding)
                {
                    mBinding->SetSubscriptionStateChangeHandler(
                        std::move(handler));
                }
            }

            /// @brief Remove subscription state change handler
            void UnsetSubscriptionStateChangeHandler()
            {
                if (mBinding)
                {
                    mBinding->UnsetSubscriptionStateChangeHandler();
                }
            }

            /// @brief Number of free sample slots available
            /// @returns Remaining free sample capacity in receive queue.
            std::size_t GetFreeSampleCount() const noexcept
            {
                if (mBinding)
                {
                    return mBinding->GetFreeSampleCount();
                }
                return 0U;
            }

            /// @brief Replace the underlying binding (SWS_CM_00340).
            ///        Used to rebind after service loss/rediscovery.
            /// @param binding New binding instance.
            void Reinit(
                std::unique_ptr<internal::ProxyEventBinding> binding) noexcept
            {
                mBinding = std::move(binding);
            }
        };

        /// @brief Skeleton-side event wrapper per AUTOSAR AP SWS_CM_00302.
        ///        This is a typed member of a generated skeleton class.
        /// @tparam T Event data type
        template <typename T>
        class SkeletonEvent
        {
        private:
            std::unique_ptr<internal::SkeletonEventBinding> mBinding;
            std::function<void(std::uint16_t, bool)> mSubscriptionHandler;

        public:
            /// @brief Construct from a binding implementation
            /// @param binding Skeleton event binding instance.
            explicit SkeletonEvent(
                std::unique_ptr<internal::SkeletonEventBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            SkeletonEvent(const SkeletonEvent &) = delete;
            SkeletonEvent &operator=(const SkeletonEvent &) = delete;
            /// @brief Move constructor.
            SkeletonEvent(SkeletonEvent &&) noexcept = default;
            /// @brief Move assignment.
            /// @returns Reference to `*this`.
            SkeletonEvent &operator=(SkeletonEvent &&) noexcept = default;

            /// @brief Allocate a sample for zero-copy send
            /// @returns SampleAllocateePtr for in-place construction
            core::Result<SampleAllocateePtr<T>> Allocate()
            {
                if (!mBinding)
                {
                    return core::Result<SampleAllocateePtr<T>>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                auto allocResult = mBinding->Allocate(sizeof(T));
                if (!allocResult.HasValue())
                {
                    return core::Result<SampleAllocateePtr<T>>::FromError(
                        allocResult.Error());
                }

                void *rawPtr = allocResult.Value();
                T *typedPtr = new (rawPtr) T{};

                auto binding = mBinding.get();
                auto deleter = [binding](T *ptr)
                {
                    ptr->~T();
                    // Note: if SendAllocated is not called, we need to free
                    std::free(ptr);
                };

                return core::Result<SampleAllocateePtr<T>>::FromValue(
                    SampleAllocateePtr<T>{
                        std::unique_ptr<T, std::function<void(T *)>>(
                            typedPtr, deleter)});
            }

            /// @brief Send a pre-allocated sample (zero-copy path)
            /// @param data Allocated sample that will be handed to binding.
            void Send(SampleAllocateePtr<T> data)
            {
                if (mBinding && data)
                {
                    T *rawPtr = data.Release();
                    mBinding->SendAllocated(rawPtr, sizeof(T));
                }
            }

            /// @brief Send by copy (standard path)
            /// @param data Sample value to serialize and publish.
            void Send(const T &data)
            {
                if (mBinding)
                {
                    auto serialized = Serializer<T>::Serialize(data);
                    mBinding->Send(serialized);
                }
            }

            /// @brief Offer this event
            /// @returns `Result<void>` indicating offer success/failure.
            core::Result<void> Offer()
            {
                if (mBinding)
                {
                    return mBinding->Offer();
                }
                return core::Result<void>::FromError(
                    MakeErrorCode(ComErrc::kServiceNotOffered));
            }

            /// @brief Stop offering this event
            void StopOffer()
            {
                if (mBinding)
                {
                    mBinding->StopOffer();
                }
            }

            /// @brief Set handler for subscription state changes (SWS_CM_00350).
            ///        Called when a client subscribes or unsubscribes to this event.
            /// @param handler Callback: (clientId, isSubscribed).
            void SetSubscriptionStateChangeHandler(
                std::function<void(std::uint16_t, bool)> handler)
            {
                mSubscriptionHandler = std::move(handler);
            }

            /// @brief Remove subscription state change handler.
            void UnsetSubscriptionStateChangeHandler()
            {
                mSubscriptionHandler = nullptr;
            }

            /// @brief Notify that a client changed subscription state.
            /// @param clientId Client identifier.
            /// @param subscribed true if subscribing, false if unsubscribing.
            void NotifySubscriptionStateChange(
                std::uint16_t clientId, bool subscribed)
            {
                if (mSubscriptionHandler)
                {
                    mSubscriptionHandler(clientId, subscribed);
                }
            }
        };
    }
}

#endif
