/// @file src/ara/phm/supervised_entity.cpp
/// @brief Implementation for supervised entity.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./supervised_entity.h"

#include <stdexcept>
#include <utility>

namespace ara
{
    namespace phm
    {
        SupervisedEntity::SupervisedEntity(
            const core::InstanceSpecifier &instance,
            CheckpointCommunicator *communicator) : mInstance{instance},
                                                    mCommunicator{communicator}
        {
            if (mCommunicator == nullptr)
            {
                throw std::invalid_argument(
                    "Checkpoint communicator must not be null.");
            }
        }

        SupervisedEntity::SupervisedEntity(
            SupervisedEntity &&se) noexcept : mInstance{std::move(se.mInstance)},
                                              mCommunicator{se.mCommunicator}
        {
        }
    }
}
