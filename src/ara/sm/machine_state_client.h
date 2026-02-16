/// @file src/ara/sm/machine_state_client.h
/// @brief Declarations for machine state client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef MACHINE_STATE_CLIENT_H
#define MACHINE_STATE_CLIENT_H

#include <cstdint>
#include <functional>
#include <mutex>
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Machine lifecycle state.
        enum class MachineState : uint8_t
        {
            kStartup = 0,  ///< Machine is starting up.
            kRunning = 1,   ///< Machine is in normal operation.
            kShutdown = 2,  ///< Machine is shutting down.
            kRestart = 3,   ///< Machine is restarting.
            kSuspend = 4    ///< Machine is suspending.
        };

        /// @brief Machine lifecycle state management client.
        class MachineStateClient
        {
        public:
            /// @brief Callback type for machine state changes.
            using StateChangeNotifier = std::function<void(MachineState)>;

            /// @brief Constructor. Initial state is kStartup.
            MachineStateClient();

            /// @brief Get the current machine state.
            /// @returns Current machine state.
            core::Result<MachineState> GetMachineState() const;

            /// @brief Register a notifier for machine state changes.
            /// @param notifier Callback invoked when state transitions occur.
            /// @returns Void Result on success, error if notifier is empty.
            core::Result<void> SetNotifier(StateChangeNotifier notifier);

            /// @brief Remove the machine state change notifier.
            void ClearNotifier() noexcept;

            /// @brief Set the machine state (platform-side API).
            /// @param state New machine state.
            /// @returns Void Result on success, kAlreadyInState if already
            ///          in the requested state.
            core::Result<void> SetMachineState(MachineState state);

        private:
            MachineState mState;
            StateChangeNotifier mNotifier;
            mutable std::mutex mMutex;
        };
    }
}

#endif
