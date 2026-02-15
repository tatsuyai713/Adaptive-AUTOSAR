/// @file src/ara/phm/checkpoint_communicator.cpp
/// @brief Implementation for checkpoint communicator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./checkpoint_communicator.h"

namespace ara
{
    namespace phm
    {
        void CheckpointCommunicator::SetCallback(CheckpointReception &&callback)
        {
            Callback = std::move(callback);
        }

        void CheckpointCommunicator::ResetCallback() noexcept
        {
            Callback = nullptr;
        }

        CheckpointCommunicator::~CheckpointCommunicator() noexcept
        {
            ResetCallback();
        }
    }
}