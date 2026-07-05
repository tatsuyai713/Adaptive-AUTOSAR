/// @file src/ara/com/someip/sd/someip_sd_client.cpp
/// @brief Implementation for someip sd client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "../../entry/service_entry.h"
#include "../../option/ipv4_endpoint_option.h"
#include "./someip_sd_client.h"

#if ARA_COM_USE_VSOMEIP

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                SomeIpSdClient::SomeIpSdClient(
                    helper::NetworkLayer<SomeIpSdMessage> *networkLayer,
                    uint16_t serviceId,
                    int initialDelayMin,
                    int initialDelayMax,
                    int repetitionBaseDelay,
                    uint32_t repetitionMax,
                    core::Optional<std::string> offeredIpAddress,
                    core::Optional<uint16_t> offeredPort) : SomeIpSdAgent<helper::SdClientState>(
                                                                networkLayer,
                                                                helper::SdClientState::ServiceNotSeen),
                                                            mServiceId{serviceId},
                                                            mApplication{nullptr},
                                                            mServiceOffered{false},
                                                            mServiceRequested{false},
                                                            mOfferedIpAddress{offeredIpAddress},
                                                            mOfferedPort{offeredPort}
                {
                    (void)initialDelayMin;
                    (void)initialDelayMax;
                    (void)repetitionBaseDelay;
                    (void)repetitionMax;

                    if (CommunicationLayer != nullptr)
                    {
                        auto receiver =
                            std::bind(
                                &SomeIpSdClient::onMessageReceived,
                                this,
                                std::placeholders::_1);
                        CommunicationLayer->SetReceiver(this, receiver);
                    }
                }

                void SomeIpSdClient::sendFindService()
                {
                    if (CommunicationLayer == nullptr)
                    {
                        return;
                    }

                    SomeIpSdMessage message;
                    message.AddEntry(
                        entry::ServiceEntry::CreateFindServiceEntry(mServiceId));
                    CommunicationLayer->Send(message);
                }

                void SomeIpSdClient::onMessageReceived(SomeIpSdMessage &&message)
                {
                    core::Optional<std::string> offeredIpAddress;
                    core::Optional<uint16_t> offeredPort;
                    bool hasRelevantOffer{false};
                    bool available{false};

                    for (const auto &entryPtr : message.Entries())
                    {
                        const auto *serviceEntry =
                            dynamic_cast<const entry::ServiceEntry *>(
                                entryPtr.get());
                        if (serviceEntry == nullptr ||
                            serviceEntry->Type() != entry::EntryType::Offering ||
                            serviceEntry->ServiceId() != mServiceId)
                        {
                            continue;
                        }

                        hasRelevantOffer = true;
                        available = serviceEntry->TTL() > 0U;

                        if (available)
                        {
                            for (const auto &optionPtr : serviceEntry->FirstOptions())
                            {
                                const auto *endpoint =
                                    dynamic_cast<const option::Ipv4EndpointOption *>(
                                        optionPtr.get());
                                if (endpoint != nullptr)
                                {
                                    offeredIpAddress =
                                        endpoint->IpAddress().ToString();
                                    offeredPort = endpoint->Port();
                                    break;
                                }
                            }
                        }
                        break;
                    }

                    if (!hasRelevantOffer)
                    {
                        return;
                    }

                    std::lock_guard<std::mutex> stateLock(mStateEventMutex);
                    mServiceOffered = available;
                    if (offeredIpAddress && offeredPort)
                    {
                        mOfferedIpAddress = offeredIpAddress.Value();
                        mOfferedPort = offeredPort.Value();
                    }

                    if (mServiceRequested)
                    {
                        SetState(
                            available ? helper::SdClientState::ServiceReady
                                      : helper::SdClientState::Stopped);
                    }
                    else
                    {
                        SetState(
                            available ? helper::SdClientState::ServiceSeen
                                      : helper::SdClientState::ServiceNotSeen);
                    }

                    if (available)
                    {
                        mOfferingConditionVariable.notify_all();
                    }
                    else
                    {
                        mStopOfferingConditionVariable.notify_all();
                    }
                }

                void SomeIpSdClient::ensureApplication()
                {
                    if (mApplication)
                    {
                        return;
                    }

                    mApplication = VsomeipApplication::GetClientApplication();
                    if (!mApplication)
                    {
                        throw std::runtime_error(
                            "vsomeip client application could not be created.");
                    }

                    mApplication->register_availability_handler(
                        mServiceId,
                        vsomeip::ANY_INSTANCE,
                        std::bind(
                            &SomeIpSdClient::onAvailability,
                            this,
                            std::placeholders::_1,
                            std::placeholders::_2,
                            std::placeholders::_3));

                    mApplication->register_state_handler(
                        [this](vsomeip::state_type_e state)
                        {
                            if (state == vsomeip::state_type_e::ST_DEREGISTERED)
                            {
                                std::lock_guard<std::mutex> _stateLock(mStateEventMutex);
                                if (mServiceOffered)
                                {
                                    mServiceOffered = false;
                                    SetState(
                                        mServiceRequested
                                            ? helper::SdClientState::Stopped
                                            : helper::SdClientState::ServiceNotSeen);
                                    mStopOfferingConditionVariable.notify_all();
                                }
                            }
                        });
                }

                void SomeIpSdClient::onAvailability(
                    vsomeip::service_t service,
                    vsomeip::instance_t instance,
                    bool isAvailable)
                {
                    (void)service;
                    (void)instance;

                    std::lock_guard<std::mutex> _stateLock(mStateEventMutex);
                    mServiceOffered = isAvailable;

                    if (mServiceRequested)
                    {
                        SetState(
                            isAvailable ? helper::SdClientState::ServiceReady
                                        : helper::SdClientState::Stopped);
                    }
                    else
                    {
                        SetState(
                            isAvailable ? helper::SdClientState::ServiceSeen
                                        : helper::SdClientState::ServiceNotSeen);
                    }

                    if (isAvailable)
                    {
                        mOfferingConditionVariable.notify_all();
                    }
                    else
                    {
                        mStopOfferingConditionVariable.notify_all();
                    }
                }

                bool SomeIpSdClient::waitForCondition(
                    std::condition_variable &conditionVariable,
                    int duration,
                    std::function<bool()> predicate)
                {
                    std::unique_lock<std::mutex> _stateLock(mStateEventMutex);

                    if (duration > 0)
                    {
                        return conditionVariable.wait_for(
                            _stateLock,
                            std::chrono::milliseconds(duration),
                            predicate);
                    }
                    else
                    {
                        conditionVariable.wait(_stateLock, predicate);
                        return true;
                    }
                }

                void SomeIpSdClient::StartAgent(helper::SdClientState state)
                {
                    (void)state;

                    if (CommunicationLayer != nullptr)
                    {
                        {
                            std::lock_guard<std::mutex> stateLock(mStateEventMutex);
                            mServiceRequested = true;
                            SetState(
                                mServiceOffered
                                    ? helper::SdClientState::ServiceReady
                                    : helper::SdClientState::InitialWaitPhase);
                        }

                        sendFindService();
                        return;
                    }

                    ensureApplication();

                    {
                        std::lock_guard<std::mutex> _stateLock(mStateEventMutex);
                        mServiceRequested = true;
                        SetState(helper::SdClientState::InitialWaitPhase);
                    }

                    mApplication->request_service(mServiceId, vsomeip::ANY_INSTANCE);
                }

                void SomeIpSdClient::StopAgent()
                {
                    {
                        std::lock_guard<std::mutex> _stateLock(mStateEventMutex);
                        mServiceRequested = false;
                        SetState(
                            mServiceOffered ? helper::SdClientState::ServiceSeen
                                            : helper::SdClientState::ServiceNotSeen);
                    }

                    if (CommunicationLayer == nullptr && mApplication)
                    {
                        mApplication->release_service(mServiceId, vsomeip::ANY_INSTANCE);
                    }
                    mStopOfferingConditionVariable.notify_all();
                }

                bool SomeIpSdClient::TryWaitUntiServiceOffered(int duration)
                {
                    return waitForCondition(
                        mOfferingConditionVariable,
                        duration,
                        [this]()
                        { return mServiceOffered; });
                }

                bool SomeIpSdClient::TryWaitUntiServiceOfferStopped(int duration)
                {
                    return waitForCondition(
                        mStopOfferingConditionVariable,
                        duration,
                        [this]()
                        { return !mServiceOffered; });
                }

                bool SomeIpSdClient::TryGetOfferedEndpoint(
                    std::string &ipAddress, uint16_t &port)
                {
                    std::lock_guard<std::mutex> _stateLock(mStateEventMutex);
                    if (!(mOfferedIpAddress && mOfferedPort))
                    {
                        const char *cIpAddress{std::getenv("ADAPTIVE_AUTOSAR_SD_OFFERED_IP")};
                        const char *cPort{std::getenv("ADAPTIVE_AUTOSAR_SD_OFFERED_PORT")};
                        if (cIpAddress && cPort)
                        {
                            mOfferedIpAddress = std::string(cIpAddress);
                            mOfferedPort = static_cast<uint16_t>(std::stoi(cPort));
                        }
                    }

                    if (mOfferedIpAddress && mOfferedPort)
                    {
                        ipAddress = mOfferedIpAddress.Value();
                        port = mOfferedPort.Value();
                        return true;
                    }

                    return false;
                }

                SomeIpSdClient::~SomeIpSdClient()
                {
                    if (CommunicationLayer != nullptr)
                    {
                        CommunicationLayer->RemoveReceiver(this);
                    }

                    if (mApplication)
                    {
                        mApplication->unregister_availability_handler(
                            mServiceId, vsomeip::ANY_INSTANCE);
                    }

                    Stop();
                    Join();
                }
            }
        }
    }
}

#endif // ARA_COM_USE_VSOMEIP
