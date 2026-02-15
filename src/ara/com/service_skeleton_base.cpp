#include <algorithm>
#include <utility>

#include "./service_skeleton_base.h"
#include "./com_error_domain.h"
#include "./someip/vsomeip_application.h"

namespace ara
{
    namespace com
    {
        ServiceSkeletonBase::ServiceSkeletonBase(
            core::InstanceSpecifier specifier,
            std::uint16_t serviceId,
            std::uint16_t instanceId,
            std::uint8_t majorVersion,
            std::uint32_t minorVersion,
            MethodCallProcessingMode mode) noexcept
            : mInstanceSpecifier{std::move(specifier)},
              mServiceId{serviceId},
              mInstanceId{instanceId},
              mMajorVersion{majorVersion},
              mMinorVersion{minorVersion},
              mProcessingMode{mode}
        {
        }

        ServiceSkeletonBase::~ServiceSkeletonBase() noexcept
        {
            if (mOffered)
            {
                StopOfferService();
            }
        }

        std::uint16_t ServiceSkeletonBase::GetServiceId() const noexcept
        {
            return mServiceId;
        }

        std::uint16_t ServiceSkeletonBase::GetInstanceId() const noexcept
        {
            return mInstanceId;
        }

        core::Result<void> ServiceSkeletonBase::OfferService()
        {
            if (!mOffered)
            {
                auto app = someip::VsomeipApplication::GetServerApplication();
                app->offer_service(
                    mServiceId,
                    mInstanceId,
                    static_cast<vsomeip::major_version_t>(mMajorVersion),
                    static_cast<vsomeip::minor_version_t>(mMinorVersion));
                mOffered = true;
            }

            return core::Result<void>::FromValue();
        }

        void ServiceSkeletonBase::StopOfferService()
        {
            if (mOffered)
            {
                auto app = someip::VsomeipApplication::GetServerApplication();

                std::vector<std::uint16_t> eventGroups;
                {
                    std::lock_guard<std::mutex> lock(mSubscriptionMutex);
                    eventGroups = mRegisteredEventGroups;
                    mRegisteredEventGroups.clear();
                }

                for (const auto eventGroupId : eventGroups)
                {
                    app->unregister_subscription_handler(
                        mServiceId,
                        mInstanceId,
                        static_cast<vsomeip::eventgroup_t>(eventGroupId));
                }

                app->stop_offer_service(
                    mServiceId,
                    mInstanceId,
                    static_cast<vsomeip::major_version_t>(mMajorVersion),
                    static_cast<vsomeip::minor_version_t>(mMinorVersion));
                mOffered = false;
            }
        }

        core::Result<void> ServiceSkeletonBase::SetEventSubscriptionStateHandler(
            std::uint16_t eventGroupId,
            EventSubscriptionStateHandler handler)
        {
            if (!handler)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ComErrc::kSetHandlerNotSet));
            }

            if (!mOffered)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(ComErrc::kServiceNotOffered));
            }

            {
                std::lock_guard<std::mutex> lock(mSubscriptionMutex);
                const auto registeredIt = std::find(
                    mRegisteredEventGroups.begin(),
                    mRegisteredEventGroups.end(),
                    eventGroupId);
                if (registeredIt != mRegisteredEventGroups.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }
            }

            auto app = someip::VsomeipApplication::GetServerApplication();
            app->register_subscription_handler(
                mServiceId,
                mInstanceId,
                static_cast<vsomeip::eventgroup_t>(eventGroupId),
                [handler = std::move(handler)](
                    vsomeip::client_t client,
                    const vsomeip_sec_client_t *,
                    const std::string &,
                    bool subscribed)
                {
                    return handler(
                        static_cast<std::uint16_t>(client),
                        subscribed);
                });

            {
                std::lock_guard<std::mutex> lock(mSubscriptionMutex);
                mRegisteredEventGroups.push_back(eventGroupId);
            }

            return core::Result<void>::FromValue();
        }

        void ServiceSkeletonBase::UnsetEventSubscriptionStateHandler(
            std::uint16_t eventGroupId)
        {
            bool wasRegistered{false};
            {
                std::lock_guard<std::mutex> lock(mSubscriptionMutex);
                auto registeredIt = std::find(
                    mRegisteredEventGroups.begin(),
                    mRegisteredEventGroups.end(),
                    eventGroupId);
                if (registeredIt != mRegisteredEventGroups.end())
                {
                    mRegisteredEventGroups.erase(registeredIt);
                    wasRegistered = true;
                }
            }

            if (!wasRegistered)
            {
                return;
            }

            auto app = someip::VsomeipApplication::GetServerApplication();
            app->unregister_subscription_handler(
                mServiceId,
                mInstanceId,
                static_cast<vsomeip::eventgroup_t>(eventGroupId));
        }

        bool ServiceSkeletonBase::IsOffered() const noexcept
        {
            return mOffered;
        }

        const core::InstanceSpecifier &
        ServiceSkeletonBase::GetInstanceSpecifier() const noexcept
        {
            return mInstanceSpecifier;
        }
    }
}
