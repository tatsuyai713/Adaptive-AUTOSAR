/// @file src/ara/com/someip/pubsub/fsm/service_down_state.cpp
/// @brief Implementation for service down state.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./service_down_state.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                namespace fsm
                {
                    ServiceDownState::ServiceDownState() noexcept : helper::MachineState<helper::PubSubState>(helper::PubSubState::ServiceDown)
                    {
                    }

                    void ServiceDownState::Activate(helper::PubSubState previousState)
                    {
                        // Nothing to do on activation
                    }

                    void ServiceDownState::Started()
                    {
                        Transit(helper::PubSubState::NotSubscribed);
                    }

                    void ServiceDownState::Deactivate(helper::PubSubState nextState)
                    {
                        // Nothing to do on deactivation
                    }
                }
            }
        }
    }
}