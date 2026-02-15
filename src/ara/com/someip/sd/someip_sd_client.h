/// @file src/ara/com/someip/sd/someip_sd_client.h
/// @brief Declarations for someip sd client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SOMEIP_SD_CLIENT
#define SOMEIP_SD_CLIENT

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include "../../../core/optional.h"
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
                /// @brief SOME/IP service discovery client based on vsomeip availability.
                class SomeIpSdClient : public SomeIpSdAgent<helper::SdClientState>
                {
                private:
                    const uint16_t mServiceId;
                    std::shared_ptr<vsomeip::application> mApplication;
                    std::mutex mStateEventMutex;
                    std::condition_variable mOfferingConditionVariable;
                    std::condition_variable mStopOfferingConditionVariable;
                    bool mServiceOffered;
                    bool mServiceRequested;
                    core::Optional<std::string> mOfferedIpAddress;
                    core::Optional<uint16_t> mOfferedPort;

                    void ensureApplication();

                    void onAvailability(
                        vsomeip::service_t service,
                        vsomeip::instance_t instance,
                        bool isAvailable);

                    bool waitForCondition(
                        std::condition_variable &conditionVariable,
                        int duration,
                        std::function<bool()> predicate);

                protected:
                    void StartAgent(helper::SdClientState state) override;
                    void StopAgent() override;

                public:
                    SomeIpSdClient() = delete;

                    SomeIpSdClient(
                        helper::NetworkLayer<SomeIpSdMessage> *networkLayer,
                        uint16_t serviceId,
                        int initialDelayMin,
                        int initialDelayMax,
                        int repetitionBaseDelay = 30,
                        uint32_t repetitionMax = 3,
                        core::Optional<std::string> offeredIpAddress = {},
                        core::Optional<uint16_t> offeredPort = {});

                    bool TryWaitUntiServiceOffered(int duration);

                    bool TryWaitUntiServiceOfferStopped(int duration);

                    bool TryGetOfferedEndpoint(
                        std::string &ipAddress, uint16_t &port);

                    ~SomeIpSdClient() override;
                };
            }
        }
    }
}

#endif
