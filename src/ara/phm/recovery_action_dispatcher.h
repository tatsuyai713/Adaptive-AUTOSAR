#ifndef RECOVERY_ACTION_DISPATCHER_H
#define RECOVERY_ACTION_DISPATCHER_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include "../core/result.h"
#include "./phm_error_domain.h"
#include "./recovery_action.h"

namespace ara
{
    namespace phm
    {
        /// @brief Dispatcher that manages and invokes registered RecoveryAction
        ///        instances by name.
        /// @note Repository helper for wiring recovery-action dispatch policies.
        class RecoveryActionDispatcher
        {
        private:
            std::unordered_map<std::string, RecoveryAction *> mActions;

        public:
            RecoveryActionDispatcher() = default;
            ~RecoveryActionDispatcher() = default;

            /// @brief Register a recovery action under a given name.
            /// @param name Unique name to identify the action.
            /// @param action Pointer to the action (ownership is NOT taken).
            /// @returns Already exists on duplicate name, invalid argument on empty name or null action.
            core::Result<void> Register(
                const std::string &name,
                RecoveryAction *action);

            /// @brief Unregister a previously registered recovery action.
            /// @param name Name of the action to remove.
            /// @returns Not found if entry does not exist.
            core::Result<void> Unregister(const std::string &name);

            /// @brief Dispatch a recovery event to the named action.
            /// @param name Name of the registered action to invoke.
            /// @param executionError The error event to pass to the handler.
            /// @param supervision The supervision type that triggered recovery.
            /// @returns Not found if entry does not exist.
            core::Result<void> Dispatch(
                const std::string &name,
                const exec::ExecutionErrorEvent &executionError,
                TypeOfSupervision supervision);

            /// @brief Get the number of registered actions.
            /// @returns Number of currently registered actions.
            std::size_t GetActionCount() const noexcept;
        };
    }
}

#endif
