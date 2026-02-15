/// @file src/ara/phm/checkpoint_communicator.h
/// @brief Declarations for checkpoint communicator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CHECKPOINT_COMMUNICATOR_H
#define CHECKPOINT_COMMUNICATOR_H

#include <stdint.h>
#include <functional>
#include "../core/result.h"
#include "./phm_error_domain.h"

namespace ara
{
    /// @brief Platform Health Monitoring functional cluster namespace
    namespace phm
    {
        /// @brief An abstract class to communicate a checkpoint between an application and the PHM cluster
        /// @note Repository helper abstraction for checkpoint transport wiring.
        class CheckpointCommunicator
        {
        public:
            /// @brief Callback type for checkpoint reception
            using CheckpointReception = std::function<void(uint32_t)>;

        protected:
            /// @brief Callback to be invoked at a checkpoint reception
            CheckpointReception Callback;

            CheckpointCommunicator() noexcept = default;

        public:
            /// @brief Try to send a checkpoint occurrence
            /// @param checkpoint Occurred checkpoint
            /// @returns True if the checkpoint is successfully queued for sending; otherwise false
            virtual bool TrySend(uint32_t checkpoint) = 0;

            /// @brief Set a callback to be invoked at a checkpoint reception
            /// @param callback Callback to be invoked
            /// @returns Invalid argument if callback is empty
            core::Result<void> SetCallback(CheckpointReception callback);

            /// @brief Reset the callback to be invoked at a checkpoint reception
            void ResetCallback() noexcept;

            virtual ~CheckpointCommunicator() noexcept;
        };
    }
}

#endif
