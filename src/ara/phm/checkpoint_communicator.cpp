/// @file src/ara/phm/checkpoint_communicator.cpp
/// @brief Implementation for checkpoint communicator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./checkpoint_communicator.h"

namespace ara
{
    namespace phm
    {
        core::Result<void> CheckpointCommunicator::SetCallback(CheckpointReception callback)
        {
            if (!callback)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kInvalidArgument));
            }

            Callback = std::move(callback);
            return core::Result<void>{};
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
