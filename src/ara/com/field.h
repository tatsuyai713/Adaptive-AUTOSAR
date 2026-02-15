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
            ProxyField(ProxyField &&) noexcept = default;
            ProxyField &operator=(ProxyField &&) noexcept = default;

            /// @brief Get field value from server
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

            void Subscribe(std::size_t maxSampleCount)
            {
                if (mHasNotifier)
                {
                    mNotifier.Subscribe(maxSampleCount);
                }
            }

            void Unsubscribe()
            {
                if (mHasNotifier)
                {
                    mNotifier.Unsubscribe();
                }
            }

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

            void SetReceiveHandler(EventReceiveHandler handler)
            {
                if (mHasNotifier)
                {
                    mNotifier.SetReceiveHandler(std::move(handler));
                }
            }

            void UnsetReceiveHandler()
            {
                if (mHasNotifier)
                {
                    mNotifier.UnsetReceiveHandler();
                }
            }

            void SetSubscriptionStateChangeHandler(
                SubscriptionStateChangeHandler handler)
            {
                if (mHasNotifier)
                {
                    mNotifier.SetSubscriptionStateChangeHandler(
                        std::move(handler));
                }
            }

            void UnsetSubscriptionStateChangeHandler()
            {
                if (mHasNotifier)
                {
                    mNotifier.UnsetSubscriptionStateChangeHandler();
                }
            }

            SubscriptionState GetSubscriptionState() const noexcept
            {
                if (!mHasNotifier)
                {
                    return SubscriptionState::kNotSubscribed;
                }

                return mNotifier.GetSubscriptionState();
            }

            std::size_t GetFreeSampleCount() const noexcept
            {
                if (!mHasNotifier)
                {
                    return 0U;
                }

                return mNotifier.GetFreeSampleCount();
            }

            bool HasGetter() const noexcept { return mHasGetter; }
            bool HasSetter() const noexcept { return mHasSetter; }
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
            explicit SkeletonField(
                std::unique_ptr<internal::SkeletonEventBinding> notifierBinding)
                : mNotifier{std::move(notifierBinding)}
            {
            }

            SkeletonField(const SkeletonField &) = delete;
            SkeletonField &operator=(const SkeletonField &) = delete;
            SkeletonField(SkeletonField &&) noexcept = default;
            SkeletonField &operator=(SkeletonField &&) noexcept = default;

            /// @brief Update field value and notify subscribers
            void Update(const T &value)
            {
                mValue = value;
                mNotifier.Send(value);
            }

            /// @brief Get current field value (for Get handler dispatch)
            const T &GetValue() const noexcept { return mValue; }

            /// @brief Offer the notification event
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
