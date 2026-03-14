/// @file src/ara/com/e2e/e2e_method.h
/// @brief E2E protection decorators for method bindings.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_E2E_METHOD_H
#define ARA_COM_E2E_METHOD_H

#include <memory>
#include <vector>
#include "./profile.h"
#include "../internal/method_binding.h"
#include "../com_error_domain.h"
#include "../../core/result.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            /// @brief Binding decorator that applies E2E protection on the
            ///        skeleton (method handler) side.  Intercepts Register() so
            ///        that every incoming request is E2E-checked before dispatch,
            ///        and the outgoing response is E2E-protected before return.
            class E2ESkeletonMethodBindingDecorator
                : public internal::SkeletonMethodBinding
            {
            private:
                std::unique_ptr<internal::SkeletonMethodBinding> mInner;
                Profile *mProfile;
                std::size_t mE2EHeaderSize;

            public:
                /// @param inner      Underlying skeleton method binding
                /// @param profile    E2E profile (must outlive this object)
                /// @param headerSize Bytes prepended by the profile (e.g. 2 for P11)
                E2ESkeletonMethodBindingDecorator(
                    std::unique_ptr<internal::SkeletonMethodBinding> inner,
                    Profile &profile,
                    std::size_t headerSize) noexcept
                    : mInner{std::move(inner)},
                      mProfile{&profile},
                      mE2EHeaderSize{headerSize}
                {
                }

                core::Result<void> Register(
                    RawRequestHandler handler) override
                {
                    Profile *profile = mProfile;
                    std::size_t hdrSize = mE2EHeaderSize;

                    return mInner->Register(
                        [profile, hdrSize, h = std::move(handler)](
                            const std::vector<std::uint8_t> &request)
                            -> core::Result<std::vector<std::uint8_t>>
                        {
                            // -- check incoming request --
                            std::vector<std::uint8_t> protReq(request);
                            CheckStatusType reqStatus =
                                profile->Check(protReq);

                            if (reqStatus != CheckStatusType::kOk ||
                                request.size() < hdrSize)
                            {
                                return core::Result<
                                    std::vector<std::uint8_t>>::FromError(
                                    MakeErrorCode(
                                        ComErrc::kCommunicationStackError));
                            }

                            // Strip E2E header before passing to handler
                            std::vector<std::uint8_t> stripped(
                                request.begin() +
                                    static_cast<std::ptrdiff_t>(hdrSize),
                                request.end());

                            auto result = h(stripped);
                            if (!result.HasValue())
                            {
                                return result;
                            }

                            // -- protect outgoing response --
                            std::vector<std::uint8_t> protResp;
                            if (profile->TryProtect(
                                    result.Value(), protResp))
                            {
                                return core::Result<
                                    std::vector<std::uint8_t>>::FromValue(
                                    std::move(protResp));
                            }

                            return core::Result<
                                std::vector<std::uint8_t>>::FromError(
                                MakeErrorCode(
                                    ComErrc::kCommunicationStackError));
                        });
                }

                void Unregister() override
                {
                    mInner->Unregister();
                }
            };

            /// @brief Binding decorator that applies E2E protection on the
            ///        proxy (caller) side.  Intercepts Call() so that outgoing
            ///        requests are E2E-protected and incoming responses are
            ///        E2E-checked before the application callback.
            class E2EProxyMethodBindingDecorator
                : public internal::ProxyMethodBinding
            {
            private:
                std::unique_ptr<internal::ProxyMethodBinding> mInner;
                Profile *mProfile;
                std::size_t mE2EHeaderSize;

            public:
                /// @param inner      Underlying proxy method binding
                /// @param profile    E2E profile (must outlive this object)
                /// @param headerSize Bytes prepended by the profile (e.g. 2 for P11)
                E2EProxyMethodBindingDecorator(
                    std::unique_ptr<internal::ProxyMethodBinding> inner,
                    Profile &profile,
                    std::size_t headerSize) noexcept
                    : mInner{std::move(inner)},
                      mProfile{&profile},
                      mE2EHeaderSize{headerSize}
                {
                }

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override
                {
                    // -- protect outgoing request --
                    std::vector<std::uint8_t> protReq;
                    if (!mProfile->TryProtect(requestPayload, protReq))
                    {
                        responseHandler(
                            core::Result<std::vector<std::uint8_t>>::FromError(
                                MakeErrorCode(
                                    ComErrc::kCommunicationStackError)));
                        return;
                    }

                    Profile *profile = mProfile;
                    std::size_t hdrSize = mE2EHeaderSize;

                    mInner->Call(
                        protReq,
                        [profile, hdrSize,
                         rh = std::move(responseHandler)](
                            core::Result<std::vector<std::uint8_t>> rawResp)
                        {
                            if (!rawResp.HasValue())
                            {
                                rh(std::move(rawResp));
                                return;
                            }

                            // -- check incoming response --
                            std::vector<std::uint8_t> protResp(
                                rawResp.Value());
                            CheckStatusType status =
                                profile->Check(protResp);

                            if (status != CheckStatusType::kOk ||
                                rawResp.Value().size() < hdrSize)
                            {
                                rh(core::Result<
                                    std::vector<std::uint8_t>>::FromError(
                                    MakeErrorCode(
                                        ComErrc::kCommunicationStackError)));
                                return;
                            }

                            // Strip E2E header
                            const auto &val = rawResp.Value();
                            std::vector<std::uint8_t> stripped(
                                val.begin() +
                                    static_cast<std::ptrdiff_t>(hdrSize),
                                val.end());

                            rh(core::Result<
                                std::vector<std::uint8_t>>::FromValue(
                                std::move(stripped)));
                        });
                }
            };
        }
    }
}

#endif
