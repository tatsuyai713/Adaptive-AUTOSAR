/// @file src/ara/com/someip/pubsub/fsm/notsubscribed_state.cpp
/// @brief Implementation for notsubscribed state.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./notsubscribed_state.h"

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
                    NotSubscribedState::NotSubscribedState() noexcept : helper::MachineState<helper::PubSubState>(helper::PubSubState::NotSubscribed)
                    {
                    }

                    void NotSubscribedState::Activate(helper::PubSubState previousState)
                    {
                        // Nothing to do on activation
                    }

                    void NotSubscribedState::Subscribed()
                    {
                        Transit(helper::PubSubState::Subscribed);
                    }

                    void NotSubscribedState::Stopped()
                    {
                        Transit(helper::PubSubState::ServiceDown);
                    }

                    void NotSubscribedState::Deactivate(helper::PubSubState nextState)
                    {
                        // Nothing to do on deactivation
                    }
                }
            }
        }
    }
}