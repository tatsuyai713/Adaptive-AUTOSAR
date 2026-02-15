#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "./someip_sd_client.h"

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

                    if (mApplication)
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
