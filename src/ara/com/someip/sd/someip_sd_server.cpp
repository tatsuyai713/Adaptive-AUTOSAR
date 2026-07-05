/// @file src/ara/com/someip/sd/someip_sd_server.cpp
/// @brief Implementation for someip sd server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <cstdlib>
#include "../../entry/service_entry.h"
#include "../../option/ipv4_endpoint_option.h"
#include "./someip_sd_server.h"

#if ARA_COM_USE_VSOMEIP

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                SomeIpSdServer::SomeIpSdServer(
                    helper::NetworkLayer<SomeIpSdMessage> *networkLayer,
                    uint16_t serviceId,
                    uint16_t instanceId,
                    uint8_t majorVersion,
                    uint32_t minorVersion,
                    helper::Ipv4Address ipAddress,
                    uint16_t port,
                    int initialDelayMin,
                    int initialDelayMax,
                    int repetitionBaseDelay,
                    int cycleOfferDelay,
                    uint32_t repetitionMax) : SomeIpSdAgent<helper::SdServerState>(
                                                   networkLayer,
                                                   helper::SdServerState::NotReady),
                                              mServiceId{serviceId},
                                              mInstanceId{instanceId},
                                              mMajorVersion{majorVersion},
                                              mMinorVersion{minorVersion},
                                              mOfferedIpAddress{ipAddress.ToString()},
                                              mOfferedPort{port},
                                              mOffered{false},
                                              mOwnOfferedIpEnv{false},
                                              mOwnOfferedPortEnv{false}
                {
                    (void)initialDelayMin;
                    (void)initialDelayMax;
                    (void)repetitionBaseDelay;
                    (void)cycleOfferDelay;
                    (void)repetitionMax;

                    if (CommunicationLayer != nullptr)
                    {
                        auto receiver =
                            std::bind(
                                &SomeIpSdServer::onMessageReceived,
                                this,
                                std::placeholders::_1);
                        CommunicationLayer->SetReceiver(this, receiver);
                    }
                }

                void SomeIpSdServer::sendOffer(bool available)
                {
                    if (CommunicationLayer == nullptr)
                    {
                        return;
                    }

                    SomeIpSdMessage message;
                    std::unique_ptr<entry::ServiceEntry> offerEntry{
                        available
                            ? entry::ServiceEntry::CreateOfferServiceEntry(
                                  mServiceId,
                                  mInstanceId,
                                  mMajorVersion,
                                  mMinorVersion)
                            : entry::ServiceEntry::CreateStopOfferEntry(
                                  mServiceId,
                                  mInstanceId,
                                  mMajorVersion,
                                  mMinorVersion)};

                    if (available)
                    {
                        auto endpoint =
                            option::Ipv4EndpointOption::CreateUnitcastEndpoint(
                                false,
                                helper::Ipv4Address(mOfferedIpAddress),
                                option::Layer4ProtocolType::Tcp,
                                mOfferedPort);
                        offerEntry->AddFirstOption(std::move(endpoint));
                    }

                    message.AddEntry(std::move(offerEntry));
                    CommunicationLayer->Send(message);
                }

                void SomeIpSdServer::onMessageReceived(SomeIpSdMessage &&message)
                {
                    bool shouldOffer{false};
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        if (!mOffered)
                        {
                            return;
                        }

                        for (const auto &entryPtr : message.Entries())
                        {
                            const auto *serviceEntry =
                                dynamic_cast<const entry::ServiceEntry *>(
                                    entryPtr.get());
                            if (serviceEntry == nullptr ||
                                serviceEntry->Type() != entry::EntryType::Finding)
                            {
                                continue;
                            }

                            const bool serviceMatches =
                                serviceEntry->ServiceId() == mServiceId;
                            const bool instanceMatches =
                                serviceEntry->InstanceId() == entry::Entry::cAnyInstanceId ||
                                serviceEntry->InstanceId() == mInstanceId;
                            const bool majorMatches =
                                serviceEntry->MajorVersion() == entry::Entry::cAnyMajorVersion ||
                                serviceEntry->MajorVersion() == mMajorVersion;
                            const bool minorMatches =
                                serviceEntry->MinorVersion() ==
                                    entry::ServiceEntry::cAnyMinorVersion ||
                                serviceEntry->MinorVersion() == mMinorVersion;

                            if (serviceMatches &&
                                instanceMatches &&
                                majorMatches &&
                                minorMatches)
                            {
                                shouldOffer = true;
                                break;
                            }
                        }
                    }

                    if (shouldOffer)
                    {
                        sendOffer(true);
                    }
                }

                void SomeIpSdServer::StartAgent(helper::SdServerState state)
                {
                    (void)state;

                    if (CommunicationLayer != nullptr)
                    {
                        bool shouldOffer{false};
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            if (!mOffered)
                            {
                                mOffered = true;
                                SetState(helper::SdServerState::MainPhase);
                                shouldOffer = true;
                            }
                        }

                        if (shouldOffer)
                        {
                            sendOffer(true);
                        }
                        return;
                    }

                    if (!mApplication)
                    {
                        mApplication = VsomeipApplication::GetServerApplication();
                    }

                    if (!mOffered)
                    {
                        mApplication->offer_service(mServiceId, mInstanceId);
                        if (std::getenv("ADAPTIVE_AUTOSAR_SD_OFFERED_IP") == nullptr)
                        {
                            setenv(
                                "ADAPTIVE_AUTOSAR_SD_OFFERED_IP",
                                mOfferedIpAddress.c_str(),
                                0);
                            mOwnOfferedIpEnv = true;
                        }

                        if (std::getenv("ADAPTIVE_AUTOSAR_SD_OFFERED_PORT") == nullptr)
                        {
                            const std::string cPort{std::to_string(mOfferedPort)};
                            setenv(
                                "ADAPTIVE_AUTOSAR_SD_OFFERED_PORT",
                                cPort.c_str(),
                                0);
                            mOwnOfferedPortEnv = true;
                        }

                        mOffered = true;
                        SetState(helper::SdServerState::MainPhase);
                    }
                }

                void SomeIpSdServer::StopAgent()
                {
                    if (CommunicationLayer != nullptr)
                    {
                        bool shouldStopOffer{false};
                        {
                            std::lock_guard<std::mutex> lock(mMutex);
                            shouldStopOffer = mOffered;
                            mOffered = false;
                            SetState(helper::SdServerState::NotReady);
                        }

                        if (shouldStopOffer)
                        {
                            sendOffer(false);
                        }
                        return;
                    }

                    if (mApplication && mOffered)
                    {
                        mApplication->stop_offer_service(mServiceId, mInstanceId);
                        mOffered = false;
                    }

                    mApplication = nullptr;
                    VsomeipApplication::StopServerApplication();

                    if (mOwnOfferedIpEnv)
                    {
                        unsetenv("ADAPTIVE_AUTOSAR_SD_OFFERED_IP");
                        mOwnOfferedIpEnv = false;
                    }

                    if (mOwnOfferedPortEnv)
                    {
                        unsetenv("ADAPTIVE_AUTOSAR_SD_OFFERED_PORT");
                        mOwnOfferedPortEnv = false;
                    }

                    SetState(helper::SdServerState::NotReady);
                }

                SomeIpSdServer::~SomeIpSdServer()
                {
                    Stop();
                    if (CommunicationLayer != nullptr)
                    {
                        CommunicationLayer->RemoveReceiver(this);
                    }
                }
            }
        }
    }
}

#endif // ARA_COM_USE_VSOMEIP
