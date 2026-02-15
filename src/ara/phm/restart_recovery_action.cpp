#include "./restart_recovery_action.h"

#include <stdexcept>

namespace ara
{
    namespace phm
    {
        RestartRecoveryAction::RestartRecoveryAction(
            const core::InstanceSpecifier &instance,
            RestartCallback restartCallback)
            : RecoveryAction{instance},
              mInstance{instance},
              mRestartCallback{std::move(restartCallback)}
        {
            if (!mRestartCallback)
            {
                throw std::invalid_argument(
                    "Restart callback must not be empty.");
            }
        }

        void RestartRecoveryAction::RecoveryHandler(
            const exec::ExecutionErrorEvent &executionError,
            TypeOfSupervision supervision)
        {
            if (IsOffered())
            {
                mRestartCallback(mInstance);
            }
        }
    }
}
