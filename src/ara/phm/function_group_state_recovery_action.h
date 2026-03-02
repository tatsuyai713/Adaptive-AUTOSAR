/// @file src/ara/phm/function_group_state_recovery_action.h
/// @brief Recovery action that transitions a FunctionGroup to a specific state.
/// @details Per AUTOSAR AP SWS_PHM, this recovery action requests Execution
///          Management to transition a FunctionGroup to a pre-configured state.

#ifndef FUNCTION_GROUP_STATE_RECOVERY_ACTION_H
#define FUNCTION_GROUP_STATE_RECOVERY_ACTION_H

#include <functional>
#include <string>
#include "./recovery_action.h"

namespace ara
{
    namespace phm
    {
        /// @brief Recovery action that moves a FunctionGroup to a target state
        class FunctionGroupStateRecoveryAction : public RecoveryAction
        {
        public:
            /// @brief Callback type invoked to perform the function group state transition
            using StateTransitionCallback = std::function<void(
                const std::string &functionGroup,
                const std::string &targetState)>;

        private:
            std::string mFunctionGroup;
            std::string mTargetState;
            StateTransitionCallback mCallback;

        public:
            /// @brief Constructor
            /// @param instance Instance specifier for this recovery action
            /// @param functionGroup The target function group name
            /// @param targetState The desired state to transition into
            /// @param callback Callback invoked to perform the transition
            FunctionGroupStateRecoveryAction(
                const core::InstanceSpecifier &instance,
                std::string functionGroup,
                std::string targetState,
                StateTransitionCallback callback);

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override;
        };
    }
}

#endif
