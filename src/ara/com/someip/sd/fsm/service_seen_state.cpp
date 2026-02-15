/// @file src/ara/com/someip/sd/fsm/service_seen_state.cpp
/// @brief Implementation for service seen state.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./service_seen_state.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                namespace fsm
                {
                    ServiceSeenState::ServiceSeenState(
                        helper::TtlTimer *ttlTimer,
                        std::condition_variable *conditionVariable) noexcept : helper::MachineState<helper::SdClientState>(helper::SdClientState::ServiceSeen),
                                                                               ClientServiceState(ttlTimer),
                                                                               mConditionVariable{conditionVariable}
                    {
                    }

                    helper::SdClientState ServiceSeenState::waitForNextState()
                    {
                        while (!Timer->GetRequested() && Timer->GetOffered())
                        {
                            bool _expired = Timer->WaitForExpiration();

                            if (_expired)
                            {
                                break;
                            }
                        }

                        if (Timer->GetRequested())
                        {
                            return helper::SdClientState::ServiceReady;
                        }
                        else
                        {
                            // The service is not offering anymore or the TTL is expired:
                            return helper::SdClientState::ServiceNotSeen;
                        }
                    }

                    void ServiceSeenState::Activate(helper::SdClientState previousState)
                    {
                        mConditionVariable->notify_one();
                        helper::SdClientState _nextState = waitForNextState();
                        Transit(_nextState);
                    }

                    void ServiceSeenState::Deactivate(helper::SdClientState nextState)
                    {
                    }
                }
            }
        }
    }
}