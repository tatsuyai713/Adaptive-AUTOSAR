/// @file src/ara/sm/state_transition_handler.h
/// @brief Declarations for state transition handler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef STATE_TRANSITION_HANDLER_H
#define STATE_TRANSITION_HANDLER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief State transition phase.
        enum class TransitionPhase : uint8_t
        {
            kBefore = 0, ///< Before the transition takes effect.
            kAfter = 1   ///< After the transition has taken effect.
        };

        /// @brief Function group state transition handler.
        ///
        /// Allows applications to register callbacks that are invoked
        /// when a function group undergoes a state transition.
        class StateTransitionHandler
        {
        public:
            /// @brief Callback type for state transitions.
            using TransitionCallback = std::function<void(
                const std::string &functionGroup,
                const std::string &fromState,
                const std::string &toState,
                TransitionPhase phase)>;

            /// @brief Constructor.
            StateTransitionHandler();

            /// @brief Register a transition callback for a function group.
            /// @param functionGroup Function group name.
            /// @param callback Callback invoked on state transitions.
            /// @returns Void Result on success, error on invalid arguments.
            core::Result<void> Register(
                const std::string &functionGroup,
                TransitionCallback callback);

            /// @brief Unregister the transition callback for a function group.
            /// @param functionGroup Function group name.
            void Unregister(const std::string &functionGroup);

            /// @brief Notify a state transition (platform-side API).
            /// @param functionGroup Function group name.
            /// @param fromState Previous state name.
            /// @param toState New state name.
            /// @param phase Transition phase (before or after).
            void NotifyTransition(
                const std::string &functionGroup,
                const std::string &fromState,
                const std::string &toState,
                TransitionPhase phase);

            /// @brief Check if a handler is registered for a function group.
            /// @param functionGroup Function group name.
            /// @returns True if a handler is registered.
            bool HasHandler(const std::string &functionGroup) const;

        private:
            std::map<std::string, TransitionCallback> mHandlers;
            mutable std::mutex mMutex;
        };
    }
}

#endif
