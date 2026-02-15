#ifndef RESTART_RECOVERY_ACTION_H
#define RESTART_RECOVERY_ACTION_H

#include <functional>
#include "./recovery_action.h"

namespace ara
{
    namespace phm
    {
        /// @brief A concrete recovery action that requests a process restart
        ///        via a user-supplied callback.
        /// @note Repository helper for restart recovery policy wiring.
        class RestartRecoveryAction : public RecoveryAction
        {
        public:
            using RestartCallback = std::function<void(
                const core::InstanceSpecifier &instance)>;

        private:
            const core::InstanceSpecifier mInstance;
            RestartCallback mRestartCallback;

        public:
            /// @brief Constructor
            /// @param instance Instance specifier of the supervised entity
            /// @param restartCallback Callback invoked to perform the restart
            /// @throws std::invalid_argument Throws when callback is empty
            RestartRecoveryAction(
                const core::InstanceSpecifier &instance,
                RestartCallback restartCallback);

            void RecoveryHandler(
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision) override;
        };
    }
}

#endif
