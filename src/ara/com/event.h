#ifndef ARA_COM_EVENT_H
#define ARA_COM_EVENT_H

#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include "./types.h"
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
            explicit ProxyEvent(
                std::unique_ptr<internal::ProxyEventBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            ProxyEvent(const ProxyEvent &) = delete;
            ProxyEvent &operator=(const ProxyEvent &) = delete;
            ProxyEvent(ProxyEvent &&) noexcept = default;
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

            /// @brief Unsubscribe from this event
            void Unsubscribe()
            {
                if (mBinding)
                {
                    mBinding->Unsubscribe();
                }
            }

            /// @brief Get current subscription state
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
            void SetReceiveHandler(EventReceiveHandler handler)
            {
                if (mBinding)
                {
                    mBinding->SetReceiveHandler(std::move(handler));
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
            std::size_t GetFreeSampleCount() const noexcept
            {
                if (mBinding)
                {
                    return mBinding->GetFreeSampleCount();
                }
                return 0U;
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

        public:
            /// @brief Construct from a binding implementation
            explicit SkeletonEvent(
                std::unique_ptr<internal::SkeletonEventBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            SkeletonEvent(const SkeletonEvent &) = delete;
            SkeletonEvent &operator=(const SkeletonEvent &) = delete;
            SkeletonEvent(SkeletonEvent &&) noexcept = default;
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
            void Send(SampleAllocateePtr<T> data)
            {
                if (mBinding && data)
                {
                    T *rawPtr = data.Release();
                    mBinding->SendAllocated(rawPtr, sizeof(T));
                }
            }

            /// @brief Send by copy (standard path)
            void Send(const T &data)
            {
                if (mBinding)
                {
                    auto serialized = Serializer<T>::Serialize(data);
                    mBinding->Send(serialized);
                }
            }

            /// @brief Offer this event
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
        };
    }
}

#endif
