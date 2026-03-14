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
                                                    mCommunicator{communicator},
                                                    mEnabled{true}
        {
            if (mCommunicator == nullptr)
            {
                throw std::invalid_argument(
                    "Checkpoint communicator must not be null.");
            }
        }

        SupervisedEntity::SupervisedEntity(
            SupervisedEntity &&se) noexcept : mInstance{std::move(se.mInstance)},
                                              mCommunicator{se.mCommunicator},
                                              mEnabled{se.mEnabled}
        {
        }

        core::Result<void> SupervisedEntity::Enable()
        {
            mEnabled = true;
            return core::Result<void>{};
        }

        core::Result<void> SupervisedEntity::Disable()
        {
            mEnabled = false;
            return core::Result<void>{};
        }

        core::Result<GlobalSupervisionStatus> SupervisedEntity::GetSupervisionStatus() const
        {
            if (!mEnabled)
            {
                return core::Result<GlobalSupervisionStatus>{GlobalSupervisionStatus::kDeactivated};
            }
            return core::Result<GlobalSupervisionStatus>{GlobalSupervisionStatus::kOk};
        }

        core::Result<LocalSupervisionStatus> SupervisedEntity::GetLocalSupervisionStatus(
            std::uint32_t type) const
        {
            if (!mEnabled)
            {
                return core::Result<LocalSupervisionStatus>{LocalSupervisionStatus::kDeactivated};
            }
            (void)type;
            return core::Result<LocalSupervisionStatus>{LocalSupervisionStatus::kOk};
        }

        core::Result<SupervisedEntity> SupervisedEntity::Create(
            const core::InstanceSpecifier &specifier)
        {
            // In a full platform, the factory resolves the communicator from the manifest.
            // Return an error since no checkpoint communicator can be resolved standalone.
            (void)specifier;
            return core::Result<SupervisedEntity>::FromError(
                MakeErrorCode(PhmErrc::kNotOffered));
        }
    }
}
