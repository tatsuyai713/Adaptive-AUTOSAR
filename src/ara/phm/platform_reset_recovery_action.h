/// @file src/ara/phm/platform_reset_recovery_action.h
/// @brief Recovery action that triggers a platform reset.
/// @details Per AUTOSAR AP SWS_PHM, this recovery action requests
///          a full platform (ECU) reset as a recovery measure.

#ifndef PLATFORM_RESET_RECOVERY_ACTION_H
#define PLATFORM_RESET_RECOVERY_ACTION_H

#include <functional>
#include "./recovery_action.h"

namespace ara
{
    namespace phm
    {
        /// @brief Recovery action that triggers a platform reset
        class PlatformResetRecoveryAction : public RecoveryAction
        {
        public:
            /// @brief Callback type invoked to perform the platform reset
            using ResetCallback = std::function<void()>;

        private:
            ResetCallback mResetCallback;

        public:
            /// @brief Constructor
            /// @param instance Instance specifier for this recovery action
            /// @param resetCallback Callback invoked to perform the platform reset
            PlatformResetRecoveryAction(
                const core::InstanceSpecifier &instance,
                ResetCallback resetCallback);

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override;
        };
    }
}

#endif
