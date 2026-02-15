/// @file src/ara/com/field.h
/// @brief Declarations for field.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_FIELD_H
#define ARA_COM_FIELD_H

#include <limits>
#include <memory>
#include <utility>
#include "./event.h"
#include "./serialization.h"
#include "./internal/event_binding.h"
#include "./internal/method_binding.h"
#include "../core/future.h"
#include "../core/promise.h"

namespace ara
{
    namespace com
    {
        /// @brief Proxy-side field per AUTOSAR AP.
        ///        Combines Get/Set methods with notification event.
        ///        In the AP specification, a Field has a getter, a setter,
        ///        and a notification (event) component.
        /// @tparam T Field value type
        template <typename T>
        class ProxyField
        {
        private:
            ProxyEvent<T> mNotifier;
            std::unique_ptr<internal::ProxyMethodBinding> mGetBinding;
            std::unique_ptr<internal::ProxyMethodBinding> mSetBinding;
            bool mHasGetter;
            bool mHasSetter;
            bool mHasNotifier;

        public:
            /// @brief Constructs a proxy field wrapper from notifier/getter/setter bindings.
            /// @param notifierBinding Binding for field notifier event.
            /// @param getBinding Binding for getter method.
            /// @param setBinding Binding for setter method.
            /// @param hasGetter Capability flag indicating getter support.
            /// @param hasSetter Capability flag indicating setter support.
            /// @param hasNotifier Capability flag indicating notifier support.
            ProxyField(
                std::unique_ptr<internal::ProxyEventBinding> notifierBinding,
                std::unique_ptr<internal::ProxyMethodBinding> getBinding,
                std::unique_ptr<internal::ProxyMethodBinding> setBinding,
                bool hasGetter = true,
                bool hasSetter = true,
                bool hasNotifier = true) noexcept
                : mNotifier{std::move(notifierBinding)},
                  mGetBinding{std::move(getBinding)},
                  mSetBinding{std::move(setBinding)},
                  mHasGetter{hasGetter},
                  mHasSetter{hasSetter},
                  mHasNotifier{hasNotifier}
            {
            }

            ProxyField(const ProxyField &) = delete;
            ProxyField &operator=(const ProxyField &) = delete;
            /// @brief Move constructor.
            ProxyField(ProxyField &&) noexcept = default;
            /// @brief Move assignment.
            /// @returns Reference to `*this`.
            ProxyField &operator=(ProxyField &&) noexcept = default;

            /// @brief Get field value from server
            /// @returns Future resolved with field value or communication error.
            core::Future<T> Get()
            {
                core::Promise<T> promise;
                auto future = promise.get_future();

                if (!mHasGetter)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    return future;
                }

                if (!mGetBinding)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                    return future;
                }

                mGetBinding->Call(
                    {},
                    [p = std::make_shared<core::Promise<T>>(std::move(promise))](
                        core::Result<std::vector<std::uint8_t>> rawResult) mutable
                    {
                        if (!rawResult.HasValue())
                        {
                            p->SetError(rawResult.Error());
                            return;
                        }

                        const auto &data = rawResult.Value();
                        auto deser = Serializer<T>::Deserialize(
                            data.data(), data.size());
                        if (deser.HasValue())
                        {
                            p->set_value(std::move(deser).Value());
                        }
                        else
                        {
                            p->SetError(deser.Error());
                        }
                    });

                return future;
            }

            /// @brief Set field value on server
            /// @param value New field value.
            /// @returns Future resolved when setter call completes.
            core::Future<void> Set(const T &value)
            {
                core::Promise<void> promise;
                auto future = promise.get_future();

                if (!mHasSetter)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    return future;
                }

                if (!mSetBinding)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                    return future;
                }

                auto payload = Serializer<T>::Serialize(value);
                mSetBinding->Call(
                    payload,
                    [p = std::make_shared<core::Promise<void>>(std::move(promise))](
                        core::Result<std::vector<std::uint8_t>>) mutable
                    {
                        p->set_value();
                    });

                return future;
            }

            // ── Notification (Event) capabilities ──

            /// @brief Subscribes to field notifier updates.
            /// @param maxSampleCount Maximum buffered notifier samples.
            void Subscribe(std::size_t maxSampleCount)
            {
                if (mHasNotifier)
                {
                    mNotifier.Subscribe(maxSampleCount);
                }
            }

            /// @brief Cancels field notifier subscription.
            void Unsubscribe()
            {
                if (mHasNotifier)
                {
                    mNotifier.Unsubscribe();
                }
            }

            /// @brief Fetches and dispatches pending notifier samples.
            /// @tparam F Callable receiving `SamplePtr<T>`.
            /// @param f Sample callback.
            /// @param maxNumberOfSamples Maximum samples to process.
            /// @returns Number of processed samples or an error.
            template <typename F>
            core::Result<std::size_t> GetNewSamples(
                F &&f,
                std::size_t maxNumberOfSamples =
                    std::numeric_limits<std::size_t>::max())
            {
                if (!mHasNotifier)
                {
                    return core::Result<std::size_t>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                return mNotifier.GetNewSamples(
                    std::forward<F>(f), maxNumberOfSamples);
            }

            /// @brief Sets notifier receive callback.
            /// @param handler Callback for incoming notifier data.
            void SetReceiveHandler(EventReceiveHandler handler)
            {
                if (mHasNotifier)
                {
                    mNotifier.SetReceiveHandler(std::move(handler));
                }
            }

            /// @brief Clears notifier receive callback.
            void UnsetReceiveHandler()
            {
                if (mHasNotifier)
                {
                    mNotifier.UnsetReceiveHandler();
                }
            }

            /// @brief Sets subscription state change callback.
            /// @param handler Callback for subscription state changes.
            void SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                if (mHasNotifier)
                {
                    mNotifier.SetSubscriptionStateChangeHandler(
                        std::move(handler));
                }
            }

            /// @brief Clears subscription state callback.
            void UnsetSubscriptionStateChangeHandler()
            {
                if (mHasNotifier)
                {
                    mNotifier.UnsetSubscriptionStateChangeHandler();
                }
            }

            /// @brief Returns current notifier subscription state.
            SubscriptionState GetSubscriptionState() const noexcept
            {
                if (!mHasNotifier)
                {
                    return SubscriptionState::kNotSubscribed;
                }

                return mNotifier.GetSubscriptionState();
            }

            /// @brief Returns available receive queue capacity.
            std::size_t GetFreeSampleCount() const noexcept
            {
                if (!mHasNotifier)
                {
                    return 0U;
                }

                return mNotifier.GetFreeSampleCount();
            }

            /// @brief Indicates whether getter is available.
            bool HasGetter() const noexcept { return mHasGetter; }
            /// @brief Indicates whether setter is available.
            bool HasSetter() const noexcept { return mHasSetter; }
            /// @brief Indicates whether notifier is available.
            bool HasNotifier() const noexcept { return mHasNotifier; }
        };

        /// @brief Skeleton-side field per AUTOSAR AP.
        ///        Holds the current field value and notifies subscribers on update.
        /// @tparam T Field value type
        template <typename T>
        class SkeletonField
        {
        private:
            SkeletonEvent<T> mNotifier;
            T mValue{};

        public:
            /// @brief Constructs a skeleton field wrapper.
            /// @param notifierBinding Binding for field notifier event.
            explicit SkeletonField(
                std::unique_ptr<internal::SkeletonEventBinding> notifierBinding)
                : mNotifier{std::move(notifierBinding)}
            {
            }

            SkeletonField(const SkeletonField &) = delete;
            SkeletonField &operator=(const SkeletonField &) = delete;
            /// @brief Move constructor.
            SkeletonField(SkeletonField &&) noexcept = default;
            /// @brief Move assignment.
            /// @returns Reference to `*this`.
            SkeletonField &operator=(SkeletonField &&) noexcept = default;

            /// @brief Update field value and notify subscribers
            /// @param value New field value.
            void Update(const T &value)
            {
                mValue = value;
                mNotifier.Send(value);
            }

            /// @brief Get current field value (for Get handler dispatch)
            /// @returns Last stored field value.
            const T &GetValue() const noexcept { return mValue; }

            /// @brief Offer the notification event
            /// @returns `Result<void>` indicating offer success/failure.
            core::Result<void> Offer()
            {
                return mNotifier.Offer();
            }

            /// @brief Stop offering the notification event
            void StopOffer()
            {
                mNotifier.StopOffer();
            }
        };
    }
}

#endif
