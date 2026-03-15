/// @file src/ara/com/zerocopy/zerocopy_service_discovery.cpp
/// @brief Implementation for zero-copy service discovery.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./zerocopy_service_discovery.h"
#include <algorithm>

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            namespace
            {
                bool ChannelMatches(
                    const ChannelDescriptor &a,
                    const ChannelDescriptor &b) noexcept
                {
                    return a.Service == b.Service &&
                           a.Instance == b.Instance &&
                           a.Event == b.Event;
                }
            }

            void ZeroCopyServiceDiscovery::OfferService(
                const ChannelDescriptor &channel)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (const auto &svc : mOfferedServices)
                {
                    if (ChannelMatches(svc.Channel, channel))
                    {
                        return; // already offered
                    }
                }
                DiscoveredService ds;
                ds.Channel = channel;
                ds.State = ServiceAvailability::kAvailable;
                mOfferedServices.push_back(std::move(ds));

                if (mHandler)
                {
                    mHandler(mOfferedServices.back());
                }
            }

            void ZeroCopyServiceDiscovery::StopOfferService(
                const ChannelDescriptor &channel)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                ServiceAvailabilityHandler notify;
                DiscoveredService removed;

                auto it = std::remove_if(
                    mOfferedServices.begin(), mOfferedServices.end(),
                    [&](const DiscoveredService &s)
                    {
                        return ChannelMatches(s.Channel, channel);
                    });
                if (it != mOfferedServices.end())
                {
                    removed = *it;
                    removed.State = ServiceAvailability::kNotAvailable;
                    notify = mHandler;
                    mOfferedServices.erase(it, mOfferedServices.end());
                }

                if (notify)
                {
                    notify(removed);
                }
            }

            void ZeroCopyServiceDiscovery::ServiceFound(
                const ChannelDescriptor &channel)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (auto &svc : mFoundServices)
                {
                    if (ChannelMatches(svc.Channel, channel))
                    {
                        svc.State = ServiceAvailability::kAvailable;
                        if (mHandler)
                        {
                            mHandler(svc);
                        }
                        return;
                    }
                }
                DiscoveredService ds;
                ds.Channel = channel;
                ds.State = ServiceAvailability::kAvailable;
                mFoundServices.push_back(std::move(ds));

                if (mHandler)
                {
                    mHandler(mFoundServices.back());
                }
            }

            void ZeroCopyServiceDiscovery::ServiceLost(
                const ChannelDescriptor &channel)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (auto &svc : mFoundServices)
                {
                    if (ChannelMatches(svc.Channel, channel))
                    {
                        svc.State = ServiceAvailability::kNotAvailable;
                        if (mHandler)
                        {
                            mHandler(svc);
                        }
                        return;
                    }
                }
            }

            void ZeroCopyServiceDiscovery::SetAvailabilityHandler(
                ServiceAvailabilityHandler handler)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mHandler = std::move(handler);
            }

            std::vector<DiscoveredService>
            ZeroCopyServiceDiscovery::GetOfferedServices() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mOfferedServices;
            }

            std::vector<DiscoveredService>
            ZeroCopyServiceDiscovery::GetFoundServices() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mFoundServices;
            }

            bool ZeroCopyServiceDiscovery::IsServiceOffered(
                const ChannelDescriptor &channel) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (const auto &svc : mOfferedServices)
                {
                    if (ChannelMatches(svc.Channel, channel))
                    {
                        return true;
                    }
                }
                return false;
            }

            bool ZeroCopyServiceDiscovery::IsServiceFound(
                const ChannelDescriptor &channel) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (const auto &svc : mFoundServices)
                {
                    if (ChannelMatches(svc.Channel, channel) &&
                        svc.State == ServiceAvailability::kAvailable)
                    {
                        return true;
                    }
                }
                return false;
            }
        }
    }
}
