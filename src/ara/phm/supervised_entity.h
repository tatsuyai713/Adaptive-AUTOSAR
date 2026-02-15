/// @file src/ara/phm/supervised_entity.h
/// @brief Declarations for supervised entity.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SUPERVISED_ENTITY_H
#define SUPERVISED_ENTITY_H

#include <cstdint>
#include <type_traits>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./checkpoint_communicator.h"
#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        /// @brief A class that collects and reports supervision checkpoints
        /// @note Checkpoint transport wiring is runtime specific and uses
        ///       extension interfaces under `ara::phm::extension`.
        class SupervisedEntity
        {
        private:
            const core::InstanceSpecifier &mInstance;
            CheckpointCommunicator *const mCommunicator;

        public:
            /// @brief Constructor
            /// @param instance Adaptive application instance that owns the entity
            /// @param communicator A communication medium for reporting the checkpoints
            SupervisedEntity(const core::InstanceSpecifier &instance,
                             CheckpointCommunicator *communicator);

            SupervisedEntity(SupervisedEntity &&se) noexcept;
            SupervisedEntity(const SupervisedEntity &se) = delete;
            SupervisedEntity &operator=(const SupervisedEntity &se) = delete;

            ~SupervisedEntity() noexcept = default;

            /// @brief Report a checkpoint to the PHM functional cluster
            /// @tparam EnumT Type of the checkpoint
            /// @param checkpointId ID of the checkpoint to be reported
            template <typename EnumT>
            core::Result<void> ReportCheckpoint(EnumT checkpointId)
            {
                static_assert(
                    std::is_enum<EnumT>::value,
                    "Checkpoint identifier must be an enum type.");
                static_assert(
                    std::is_same<
                        std::underlying_type_t<EnumT>,
                        std::uint32_t>::value,
                    "Checkpoint enum underlying type must be uint32_t.");

                const auto checkpoint{static_cast<std::uint32_t>(checkpointId)};
                const bool reported{mCommunicator->TrySend(checkpoint)};
                if (!reported)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PhmErrc::kCheckpointCommunicationError));
                }

                return core::Result<void>{};
            }
        };
    }
}

#endif
