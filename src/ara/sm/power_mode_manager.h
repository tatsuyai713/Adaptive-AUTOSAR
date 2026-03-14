/// @file src/ara/sm/power_mode_manager.h
/// @brief PowerModeManager and MachineStateResult per AUTOSAR AP SWS_SM.
/// @details Provides power-mode orchestration and machine state result types.

#ifndef ARA_SM_POWER_MODE_MANAGER_H
#define ARA_SM_POWER_MODE_MANAGER_H

#include <cstdint>
#include <functional>
#include <string>
#include "../core/result.h"
#include "./power_mode.h"

namespace ara
{
    namespace sm
    {
        /// @brief Detailed result of a machine state transition (SWS_SM_00100).
        enum class MachineStateResult : std::uint32_t
        {
            kSuccess = 0,            ///< Transition completed
            kInvalidState = 1,       ///< Target state is invalid
            kTransitionFailed = 2,   ///< Transition failed during execution
            kTimeout = 3,            ///< Transition timed out
            kBusy = 4               ///< Another transition is in progress
        };

        /// @brief Coordinates function group transitions in response to power mode changes (SWS_SM_00101).
        class PowerModeManager
        {
        public:
            /// @brief Callback for power mode change notification.
            using PowerModeCallback = std::function<void(PowerModeMsg)>;

            PowerModeManager() = default;
            ~PowerModeManager() noexcept = default;
            PowerModeManager(const PowerModeManager &) = delete;
            PowerModeManager &operator=(const PowerModeManager &) = delete;

            /// @brief Request a power mode change.
            /// @param mode Target power mode.
            /// @returns Response indicating result.
            core::Result<PowerModeRespMsg> RequestPowerMode(PowerModeMsg mode);

            /// @brief Get the current power mode.
            PowerModeMsg GetCurrentPowerMode() const noexcept { return mCurrentMode; }

            /// @brief Register a callback for power mode changes.
            void SetPowerModeChangeCallback(PowerModeCallback callback);

        private:
            PowerModeMsg mCurrentMode{PowerModeMsg::kOn};
            PowerModeCallback mCallback;
        };

    } // namespace sm
} // namespace ara

#endif // ARA_SM_POWER_MODE_MANAGER_H
