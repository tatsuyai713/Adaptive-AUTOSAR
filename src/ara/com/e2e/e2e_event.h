#ifndef ARA_COM_E2E_EVENT_H
#define ARA_COM_E2E_EVENT_H

#include <cstdint>
#include <memory>
#include <vector>
#include "./profile.h"
#include "../internal/event_binding.h"
#include "../com_error_domain.h"
#include "../../core/result.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            /// @brief Binding decorator that applies E2E protection on the
            ///        skeleton (send) side. Wraps an existing SkeletonEventBinding
            ///        and intercepts Send() to apply CRC/counter protection.
            ///
            /// Usage:
            /// @code
            ///   auto binding = std::make_unique<VsomeipSkeletonEventBinding>(...);
            ///   Profile11 profile;
            ///   auto e2e = std::make_unique<E2ESkeletonEventBindingDecorator>(
            ///       std::move(binding), profile, 2U);
            ///   SkeletonEvent<MyType> event(std::move(e2e));
            ///   event.Send(value);  // automatically E2E-protected
            /// @endcode
            class E2ESkeletonEventBindingDecorator
                : public internal::SkeletonEventBinding
            {
            private:
                std::unique_ptr<internal::SkeletonEventBinding> mInner;
                Profile *mProfile;

            public:
                /// @brief Construct the E2E decorator
                /// @param inner The underlying skeleton event binding
                /// @param profile E2E profile instance (must outlive this object)
                E2ESkeletonEventBindingDecorator(
                    std::unique_ptr<internal::SkeletonEventBinding> inner,
                    Profile &profile) noexcept
                    : mInner{std::move(inner)},
                      mProfile{&profile}
                {
                }

                core::Result<void> Offer() override
                {
                    return mInner->Offer();
                }

                void StopOffer() override
                {
                    mInner->StopOffer();
                }

                core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) override
                {
                    std::vector<std::uint8_t> protectedPayload;
                    if (mProfile->TryProtect(payload, protectedPayload))
                    {
                        return mInner->Send(protectedPayload);
                    }
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                core::Result<void *> Allocate(std::size_t size) override
                {
                    return mInner->Allocate(size);
                }

                core::Result<void> SendAllocated(
                    void *data, std::size_t size) override
                {
                    // For allocated (zero-copy) sends, we need to copy out,
                    // protect, and send via the normal path
                    std::vector<std::uint8_t> payload(
                        static_cast<std::uint8_t *>(data),
                        static_cast<std::uint8_t *>(data) + size);
                    std::free(data);

                    return Send(payload);
                }
            };

            /// @brief Binding decorator that applies E2E checking on the
            ///        proxy (receive) side. Wraps an existing ProxyEventBinding
            ///        and intercepts GetNewSamples() to verify CRC/counter and
            ///        strip the E2E header before deserialization.
            ///
            /// Usage:
            /// @code
            ///   auto binding = std::make_unique<VsomeipProxyEventBinding>(...);
            ///   Profile11 profile;
            ///   auto e2e = std::make_unique<E2EProxyEventBindingDecorator>(
            ///       std::move(binding), profile, 2U);
            ///   ProxyEvent<MyType> event(std::move(e2e));
            ///   event.GetNewSamples([](auto sample){ ... });  // auto-checked
            /// @endcode
            class E2EProxyEventBindingDecorator
                : public internal::ProxyEventBinding
            {
            private:
                std::unique_ptr<internal::ProxyEventBinding> mInner;
                Profile *mProfile;
                std::size_t mE2EHeaderSize;

            public:
                /// @brief Construct the E2E decorator
                /// @param inner The underlying proxy event binding
                /// @param profile E2E profile instance (must outlive this object)
                /// @param e2eHeaderSize Number of bytes the profile prepends
                ///        (e.g. 2 for Profile11: CRC byte + counter/dataID byte)
                E2EProxyEventBindingDecorator(
                    std::unique_ptr<internal::ProxyEventBinding> inner,
                    Profile &profile,
                    std::size_t e2eHeaderSize) noexcept
                    : mInner{std::move(inner)},
                      mProfile{&profile},
                      mE2EHeaderSize{e2eHeaderSize}
                {
                }

                core::Result<void> Subscribe(
                    std::size_t maxSampleCount) override
                {
                    return mInner->Subscribe(maxSampleCount);
                }

                void Unsubscribe() override
                {
                    mInner->Unsubscribe();
                }

                SubscriptionState GetSubscriptionState() const noexcept override
                {
                    return mInner->GetSubscriptionState();
                }

                core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) override
                {
                    Profile *profile = mProfile;
                    std::size_t headerSize = mE2EHeaderSize;

                    return mInner->GetNewSamples(
                        [profile, headerSize, &handler](
                            const std::uint8_t *data, std::size_t size)
                        {
                            std::vector<std::uint8_t> protectedData(
                                data, data + size);
                            CheckStatusType status =
                                profile->Check(protectedData);

                            if (status == CheckStatusType::kOk &&
                                size > headerSize)
                            {
                                handler(
                                    data + headerSize,
                                    size - headerSize);
                            }
                            // Samples with CRC errors are silently dropped
                        },
                        maxNumberOfSamples);
                }

                void SetReceiveHandler(
                    std::function<void()> handler) override
                {
                    mInner->SetReceiveHandler(std::move(handler));
                }

                void UnsetReceiveHandler() override
                {
                    mInner->UnsetReceiveHandler();
                }

                std::size_t GetFreeSampleCount() const noexcept override
                {
                    return mInner->GetFreeSampleCount();
                }

                void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) override
                {
                    mInner->SetSubscriptionStateChangeHandler(
                        std::move(handler));
                }

                void UnsetSubscriptionStateChangeHandler() override
                {
                    mInner->UnsetSubscriptionStateChangeHandler();
                }
            };
        }
    }
}

#endif
