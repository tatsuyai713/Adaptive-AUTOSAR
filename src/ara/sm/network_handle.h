/// @file src/ara/sm/network_handle.h
/// @brief Declarations for network handle.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef NETWORK_HANDLE_H
#define NETWORK_HANDLE_H

#include <cstdint>
#include <functional>
#include <mutex>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Network communication mode (SWS_SM_91002).
        enum class ComMode : uint8_t
        {
            kFull = 0,   ///< Full communication (transmit and receive).
            kSilent = 1, ///< Silent mode (receive only, no transmit).
            kNone = 2    ///< No communication.
        };

        /// @brief Network communication mode management handle.
        class NetworkHandle
        {
        public:
            /// @brief Callback type for communication mode changes.
            using ComModeNotifier = std::function<void(ComMode)>;

            /// @brief Constructor.
            /// @param instance Instance specifier identifying the network.
            explicit NetworkHandle(const core::InstanceSpecifier &instance);

            /// @brief Request a communication mode change.
            /// @param mode Desired communication mode.
            /// @returns Void Result on success, kAlreadyInState if already in
            ///          the requested mode.
            core::Result<void> RequestComMode(ComMode mode);

            /// @brief Get the current communication mode.
            /// @returns Current communication mode.
            core::Result<ComMode> GetCurrentComMode() const;

            /// @brief Register a notifier for communication mode changes.
            /// @param notifier Callback invoked when mode transitions occur.
            /// @returns Void Result on success, error if notifier is empty.
            core::Result<void> SetNotifier(ComModeNotifier notifier);

            /// @brief Remove the communication mode change notifier.
            void ClearNotifier() noexcept;

            /// @brief Get the instance specifier.
            /// @returns Reference to the InstanceSpecifier.
            const core::InstanceSpecifier &GetInstance() const noexcept;

        private:
            core::InstanceSpecifier mInstance;
            ComMode mCurrentMode;
            ComModeNotifier mNotifier;
            mutable std::mutex mMutex;
        };
    }
}

#endif
