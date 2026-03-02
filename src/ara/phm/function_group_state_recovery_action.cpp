/// @file src/ara/phm/function_group_state_recovery_action.cpp
/// @brief Implementation for FunctionGroupStateRecoveryAction.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./function_group_state_recovery_action.h"

#include <stdexcept>
#include <utility>

namespace ara
{
    namespace phm
    {
        FunctionGroupStateRecoveryAction::FunctionGroupStateRecoveryAction(
            const core::InstanceSpecifier &instance,
            std::string functionGroup,
            std::string targetState,
            StateTransitionCallback callback)
            : RecoveryAction{instance},
              mFunctionGroup{std::move(functionGroup)},
              mTargetState{std::move(targetState)},
              mCallback{std::move(callback)}
        {
            if (!mCallback)
            {
                throw std::invalid_argument(
                    "State transition callback must not be null.");
            }
        }

        void FunctionGroupStateRecoveryAction::RecoveryHandler(
            const exec::ExecutionErrorEvent &executionError,
            TypeOfSupervision supervision)
        {
            if (IsOffered() && mCallback)
            {
                mCallback(mFunctionGroup, mTargetState);
            }
        }
    }
}
