/// @file src/ara/com/service_proxy_base.cpp
/// @brief Implementation for service proxy base.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "./service_proxy_base.h"
#include "./com_error_domain.h"
#if ARA_COM_USE_VSOMEIP
#include "./someip/vsomeip_application.h"
#endif

namespace
{
    using ServiceKey = std::pair<std::uint16_t, std::uint16_t>;

    struct FindServiceSearch
    {
        std::uint64_t HandleId{0U};
        std::uint16_t ServiceId{0U};
        std::uint16_t InstanceId{0U};
        ara::com::FindServiceHandler<ara::com::ServiceHandleType> Handler;
        ara::com::ServiceHandleContainer<ara::com::ServiceHandleType> Handles;
    };

    struct FindServiceRegistration
    {
        std::uint16_t ServiceId{0U};
        std::uint16_t InstanceId{0U};
        std::vector<std::uint64_t> HandleIds;
    };

    struct FindServiceContext
    {
        std::uint64_t NextHandleId{1U};
        std::map<std::uint64_t, FindServiceSearch> Searches;
        std::map<ServiceKey, FindServiceRegistration> Registrations;
        std::mutex Mutex;
    };

    FindServiceContext &GetFindServiceContext()
    {
        static FindServiceContext context;
        return context;
    }

    ServiceKey MakeServiceKey(
        std::uint16_t serviceId,
        std::uint16_t instanceId) noexcept
    {
        return ServiceKey{serviceId, instanceId};
    }

    std::pair<std::uint16_t, std::uint16_t> ResolveServiceIdsFromSpecifier(
        const ara::core::InstanceSpecifier &specifier)
    {
        // Per SWS_CM_00122/SWS_CM_00123, resolve the InstanceSpecifier to
        // service/instance IDs. In a full deployment this would come from ARXML.
        // This educational runtime derives deterministic IDs from a stable hash.
        const std::string path{specifier.ToString()};
        std::uint32_t hash{0x811c9dc5U};
        for (const char ch : path)
        {
            hash ^= static_cast<std::uint32_t>(ch);
            hash *= 0x01000193U;
        }

        const std::uint16_t serviceId{
            static_cast<std::uint16_t>((hash >> 16) & 0xFFFFU)};
        const std::uint16_t instanceId{
            static_cast<std::uint16_t>(hash & 0xFFFFU)};
        return std::make_pair(serviceId, instanceId);
    }
}

namespace ara
{
    namespace com
    {
        ServiceProxyBase::ServiceProxyBase(
            ServiceHandleType handle) noexcept
            : mHandle{handle}
        {
        }

        ServiceProxyBase::ServiceProxyBase(
            ServiceProxyBase &&other) noexcept
            : mHandle{other.mHandle}
        {
        }

        ServiceProxyBase &ServiceProxyBase::operator=(
            ServiceProxyBase &&other) noexcept
        {
            if (this != &other)
            {
                mHandle = other.mHandle;
            }
            return *this;
        }

        ServiceProxyBase::~ServiceProxyBase() noexcept = default;

        const ServiceHandleType &ServiceProxyBase::GetHandle() const noexcept
        {
            return mHandle;
        }

        core::Result<ServiceHandleContainer<ServiceHandleType>>
        ServiceProxyBase::FindService(
            std::uint16_t serviceId,
            std::uint16_t instanceId)
        {
            const std::chrono::milliseconds discoveryWindow{500};
            ServiceHandleContainer<ServiceHandleType> handles;
            std::mutex handlesMutex;

            auto startResult = StartFindService(
                [&handles, &handlesMutex](
                    ServiceHandleContainer<ServiceHandleType> currentHandles)
                {
                    std::lock_guard<std::mutex> lock(handlesMutex);
                    handles = std::move(currentHandles);
                },
                serviceId,
                instanceId);
            if (!startResult.HasValue())
            {
                return core::Result<ServiceHandleContainer<ServiceHandleType>>::FromError(
                    startResult.Error());
            }

            std::this_thread::sleep_for(discoveryWindow);
            (void)StopFindService(startResult.Value());

            std::lock_guard<std::mutex> lock(handlesMutex);
            return core::Result<ServiceHandleContainer<ServiceHandleType>>::FromValue(handles);
        }

        core::Result<FindServiceHandle> ServiceProxyBase::StartFindService(
            std::function<void(ServiceHandleContainer<ServiceHandleType>)> handler,
            std::uint16_t serviceId,
            std::uint16_t instanceId)
        {
            if (!handler)
            {
                return core::Result<FindServiceHandle>::FromError(
                    MakeErrorCode(ComErrc::kSetHandlerNotSet));
            }

            auto &context{GetFindServiceContext()};
            std::uint64_t handleId{0U};
            bool shouldRegister{false};
            const auto key = MakeServiceKey(serviceId, instanceId);
            ServiceHandleContainer<ServiceHandleType> initialHandles;
            bool hasInitialHandles{false};
            {
                std::lock_guard<std::mutex> lock(context.Mutex);
                handleId = context.NextHandleId++;
                FindServiceSearch search{
                    handleId,
                    serviceId,
                    instanceId,
                    std::move(handler),
                    {}};

                auto &registration = context.Registrations[key];
                if (registration.HandleIds.empty())
                {
                    shouldRegister = true;
                    registration.ServiceId = serviceId;
                    registration.InstanceId = instanceId;
                }
                else
                {
                    const auto referenceHandleId = registration.HandleIds.front();
                    auto referenceIt = context.Searches.find(referenceHandleId);
                    if (referenceIt != context.Searches.end())
                    {
                        search.Handles = referenceIt->second.Handles;
                        initialHandles = search.Handles;
                        hasInitialHandles = true;
                    }
                }

                context.Searches.emplace(handleId, std::move(search));
                registration.HandleIds.push_back(handleId);
            }

            if (shouldRegister)
            {
#if ARA_COM_USE_VSOMEIP
                auto app{someip::VsomeipApplication::GetClientApplication()};
                const auto requestedInstance{
                    instanceId == 0xFFFFU
                        ? vsomeip::ANY_INSTANCE
                        : static_cast<vsomeip::instance_t>(instanceId)};

                app->register_availability_handler(
                    static_cast<vsomeip::service_t>(serviceId),
                    requestedInstance,
                    [serviceId, instanceId](
                        vsomeip::service_t,
                        vsomeip::instance_t availableInstance,
                        bool isAvailable)
                    {
                        auto &context{GetFindServiceContext()};
                        std::vector<std::pair<
                            FindServiceHandler<ServiceHandleType>,
                            ServiceHandleContainer<ServiceHandleType>>>
                            callbacks;
                        const auto key = MakeServiceKey(serviceId, instanceId);

                        {
                            std::lock_guard<std::mutex> lock(context.Mutex);
                            auto registrationIt = context.Registrations.find(key);
                            if (registrationIt == context.Registrations.end())
                            {
                                return;
                            }

                            for (auto handleId : registrationIt->second.HandleIds)
                            {
                                auto searchIt = context.Searches.find(handleId);
                                if (searchIt == context.Searches.end())
                                {
                                    continue;
                                }

                                auto &search = searchIt->second;
                                ServiceHandleType discoveredHandle{
                                    serviceId,
                                    static_cast<std::uint16_t>(availableInstance)};

                                if (isAvailable)
                                {
                                    auto existing = std::find(
                                        search.Handles.begin(),
                                        search.Handles.end(),
                                        discoveredHandle);
                                    if (existing == search.Handles.end())
                                    {
                                        search.Handles.push_back(discoveredHandle);
                                    }
                                }
                                else
                                {
                                    auto newEnd = std::remove(
                                        search.Handles.begin(),
                                        search.Handles.end(),
                                        discoveredHandle);
                                    search.Handles.erase(newEnd, search.Handles.end());
                                }

                                callbacks.emplace_back(
                                    search.Handler,
                                    search.Handles);
                            }
                        }

                        for (auto &callbackData : callbacks)
                        {
                            if (callbackData.first)
                            {
                                callbackData.first(std::move(callbackData.second));
                            }
                        }
                    });

                app->request_service(
                    static_cast<vsomeip::service_t>(serviceId),
                    requestedInstance);
#elif defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                // DDS uses data-centric discovery: topics are matched
                // automatically.  Immediately report the requested
                // service/instance as available.
                {
                    ServiceHandleType discoveredHandle{serviceId, instanceId};
                    auto &ctx{GetFindServiceContext()};
                    FindServiceHandler<ServiceHandleType> callback;
                    ServiceHandleContainer<ServiceHandleType> handles;
                    {
                        std::lock_guard<std::mutex> lock(ctx.Mutex);
                        auto searchIt = ctx.Searches.find(handleId);
                        if (searchIt != ctx.Searches.end())
                        {
                            searchIt->second.Handles.push_back(discoveredHandle);
                            handles = searchIt->second.Handles;
                            callback = searchIt->second.Handler;
                        }
                    }
                    if (callback)
                    {
                        callback(std::move(handles));
                    }
                }
#elif ARA_COM_USE_ICEORYX
                // iceoryx uses shared-memory channels matched by service
                // description.  Immediately report availability.
                {
                    ServiceHandleType discoveredHandle{serviceId, instanceId};
                    auto &ctx{GetFindServiceContext()};
                    FindServiceHandler<ServiceHandleType> callback;
                    ServiceHandleContainer<ServiceHandleType> handles;
                    {
                        std::lock_guard<std::mutex> lock(ctx.Mutex);
                        auto searchIt = ctx.Searches.find(handleId);
                        if (searchIt != ctx.Searches.end())
                        {
                            searchIt->second.Handles.push_back(discoveredHandle);
                            handles = searchIt->second.Handles;
                            callback = searchIt->second.Handler;
                        }
                    }
                    if (callback)
                    {
                        callback(std::move(handles));
                    }
                }
#endif // ARA_COM_USE_VSOMEIP / DDS / ICEORYX
            }
            else if (hasInitialHandles)
            {
                auto &contextRef{GetFindServiceContext()};
                FindServiceHandler<ServiceHandleType> callback;
                {
                    std::lock_guard<std::mutex> lock(contextRef.Mutex);
                    auto searchIt = contextRef.Searches.find(handleId);
                    if (searchIt != contextRef.Searches.end())
                    {
                        callback = searchIt->second.Handler;
                    }
                }

                if (callback)
                {
                    callback(std::move(initialHandles));
                }
            }

            return core::Result<FindServiceHandle>::FromValue(
                FindServiceHandle{handleId});
        }

        core::Result<void> ServiceProxyBase::StopFindService(
            FindServiceHandle handle)
        {
            auto &context{GetFindServiceContext()};

            std::uint16_t serviceId{0U};
            std::uint16_t instanceId{0U};
            bool shouldUnregister{false};
            {
                std::lock_guard<std::mutex> lock(context.Mutex);
                auto searchIt = context.Searches.find(handle.GetId());
                if (searchIt == context.Searches.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                }

                serviceId = searchIt->second.ServiceId;
                instanceId = searchIt->second.InstanceId;
                context.Searches.erase(searchIt);

                const auto key = MakeServiceKey(serviceId, instanceId);
                auto registrationIt = context.Registrations.find(key);
                if (registrationIt == context.Registrations.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                auto &handleIds = registrationIt->second.HandleIds;
                auto newEnd = std::remove(
                    handleIds.begin(),
                    handleIds.end(),
                    handle.GetId());
                handleIds.erase(newEnd, handleIds.end());

                if (handleIds.empty())
                {
                    context.Registrations.erase(registrationIt);
                    shouldUnregister = true;
                }
            }

            if (shouldUnregister)
            {
#if ARA_COM_USE_VSOMEIP
                auto app{someip::VsomeipApplication::GetClientApplication()};
                const auto requestedInstance{
                    instanceId == 0xFFFFU
                        ? vsomeip::ANY_INSTANCE
                        : static_cast<vsomeip::instance_t>(instanceId)};

                app->unregister_availability_handler(
                    static_cast<vsomeip::service_t>(serviceId),
                    requestedInstance);
                app->release_service(
                    static_cast<vsomeip::service_t>(serviceId),
                    requestedInstance);
#elif defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                // DDS: no explicit unregistration needed
                (void)serviceId;
                (void)instanceId;
#elif ARA_COM_USE_ICEORYX
                // iceoryx: no explicit unregistration needed
                (void)serviceId;
                (void)instanceId;
#endif
            }

            return core::Result<void>::FromValue();
        }

        void ServiceProxyBase::StopFindService()
        {
            auto &context{GetFindServiceContext()};
            std::vector<std::uint64_t> handleIds;

            {
                std::lock_guard<std::mutex> lock(context.Mutex);
                handleIds.reserve(context.Searches.size());
                for (const auto &entry : context.Searches)
                {
                    handleIds.push_back(entry.first);
                }
            }

            for (auto handleId : handleIds)
            {
                (void)StopFindService(FindServiceHandle{handleId});
            }
        }

        core::Result<ServiceHandleContainer<ServiceHandleType>>
        ServiceProxyBase::FindService(const core::InstanceSpecifier &specifier)
        {
            const auto ids = ResolveServiceIdsFromSpecifier(specifier);
            const std::uint16_t serviceId{ids.first};
            const std::uint16_t instanceId{ids.second};
            return FindService(serviceId, instanceId);
        }

        core::Result<ServiceHandleContainer<ServiceHandleType>>
        ServiceProxyBase::FindService(const InstanceIdentifier &identifier)
        {
            // Resolve InstanceIdentifier string to service/instance IDs.
            // Educational approach: parse from "someip:SID:IID" format or
            // use stable hash like InstanceSpecifier path.
            const std::string &idStr = identifier.ToString();
            const std::string prefix{"someip:"};
            if (idStr.compare(0, prefix.size(), prefix) == 0)
            {
                // Parse "someip:serviceId:instanceId"
                auto colonPos = idStr.find(':', prefix.size());
                if (colonPos != std::string::npos)
                {
                    try
                    {
                        auto serviceId = static_cast<std::uint16_t>(
                            std::stoul(idStr.substr(prefix.size(),
                                                     colonPos - prefix.size())));
                        auto instanceId = static_cast<std::uint16_t>(
                            std::stoul(idStr.substr(colonPos + 1)));
                        return FindService(serviceId, instanceId);
                    }
                    catch (...)
                    {
                        // Fall through to hash-based resolution
                    }
                }
            }

            // Fallback: use same hash as InstanceSpecifier
            const auto ids = ResolveServiceIdsFromSpecifier(
                core::InstanceSpecifier{idStr});
            return FindService(ids.first, ids.second);
        }

        core::Result<FindServiceHandle> ServiceProxyBase::StartFindService(
            std::function<void(ServiceHandleContainer<ServiceHandleType>)> handler,
            const core::InstanceSpecifier &specifier)
        {
            const auto ids = ResolveServiceIdsFromSpecifier(specifier);
            return StartFindService(
                std::move(handler),
                ids.first,
                ids.second);
        }

        bool ServiceProxyBase::CheckServiceVersion(
            const ServiceVersion &requiredVersion,
            const ServiceVersion &offeredVersion,
            VersionCheckPolicy policy) noexcept
        {
            return requiredVersion.IsCompatibleWith(offeredVersion, policy);
        }

        InstanceIdentifier ServiceProxyBase::GetServiceInstanceId() const noexcept
        {
            return InstanceIdentifier{
                mHandle.GetServiceId(),
                mHandle.GetInstanceId()};
        }
    }
}
