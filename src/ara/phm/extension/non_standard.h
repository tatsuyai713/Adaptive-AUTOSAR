/// @file src/ara/phm/extension/non_standard.h
/// @brief Extension aliases for non-standard PHM APIs.
/// @details Keep this surface minimal. Standardized APIs should be consumed
///          from `ara::phm::*` directly.

#ifndef ARA_PHM_EXTENSION_NON_STANDARD_H
#define ARA_PHM_EXTENSION_NON_STANDARD_H

#include "../recovery_action_dispatcher.h"
#include "../restart_recovery_action.h"

namespace ara
{
    namespace phm
    {
        namespace extension
        {
            /// @brief Alias to repository-specific recovery action dispatcher.
            using RecoveryActionDispatcher = ara::phm::RecoveryActionDispatcher;

            /// @brief Alias to repository-specific restart recovery action.
            using RestartRecoveryAction = ara::phm::RestartRecoveryAction;
        }
    }
}

#endif
