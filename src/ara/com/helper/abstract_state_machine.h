/// @file src/ara/com/helper/abstract_state_machine.h
/// @brief Declarations for abstract state machine.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ABSTRACT_MACHINE_STATE_H
#define ABSTRACT_MACHINE_STATE_H

namespace ara
{
    namespace com
    {
        namespace helper
        {
            /// @brief Abstract Finite State Machine (FMS) that transits between states
            /// @tparam T State enumeration type
            template <typename T>
            class AbstractStateMachine
            {
            public:
                /// @brief Tranit from the current state to a new state
                /// @param previousState Previous state that the FSM is transiting from
                /// @param nextState Next state that the FSM is transiting to
                virtual void Transit(T previousState, T nextState) = 0;
            };
        }
    }
}
#endif