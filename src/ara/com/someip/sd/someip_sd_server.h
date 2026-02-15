/// @file src/ara/com/someip/sd/someip_sd_server.h
/// @brief Declarations for someip sd server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SOMEIP_SD_SERVER
#define SOMEIP_SD_SERVER

#include <cstdint>
#include <memory>
#include <string>
#include "../../helper/ipv4_address.h"
#include "../vsomeip_application.h"
#include "./someip_sd_agent.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                /// @brief SOME/IP service discovery server based on vsomeip service offer.
                class SomeIpSdServer : public SomeIpSdAgent<helper::SdServerState>
                {
                private:
                    const uint16_t mServiceId;
                    const uint16_t mInstanceId;
                    const uint8_t mMajorVersion;
                    const uint32_t mMinorVersion;
                    std::string mOfferedIpAddress;
                    uint16_t mOfferedPort;
                    std::shared_ptr<vsomeip::application> mApplication;
                    bool mOffered;
                    bool mOwnOfferedIpEnv;
                    bool mOwnOfferedPortEnv;

                protected:
                    void StartAgent(helper::SdServerState state) override;
                    void StopAgent() override;

                public:
                    SomeIpSdServer() = delete;

                    SomeIpSdServer(
                        helper::NetworkLayer<SomeIpSdMessage> *networkLayer,
                        uint16_t serviceId,
                        uint16_t instanceId,
                        uint8_t majorVersion,
                        uint32_t minorVersion,
                        helper::Ipv4Address ipAddress,
                        uint16_t port,
                        int initialDelayMin,
                        int initialDelayMax,
                        int repetitionBaseDelay = 30,
                        int cycleOfferDelay = 1000,
                        uint32_t repetitionMax = 3);

                    ~SomeIpSdServer() override;
                };
            }
        }
    }
}

#endif
