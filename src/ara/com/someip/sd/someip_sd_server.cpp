/// @file src/ara/com/someip/sd/someip_sd_server.cpp
/// @brief Implementation for someip sd server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <cstdlib>
#include "./someip_sd_server.h"

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
                }

                void SomeIpSdServer::StartAgent(helper::SdServerState state)
                {
                    (void)state;

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
                }
            }
        }
    }
}
