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
#include "./supervision_status.h"

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
            bool mEnabled;

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
                if (!mEnabled)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PhmErrc::kCheckpointCommunicationError));
                }
                const bool reported{mCommunicator->TrySend(checkpoint)};
                if (!reported)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PhmErrc::kCheckpointCommunicationError));
                }

                return core::Result<void>{};
            }

            /// @brief Enable supervision for this entity (SWS_PHM_01140)
            /// @returns Void Result on success
            core::Result<void> Enable();

            /// @brief Disable supervision for this entity (SWS_PHM_01141)
            /// @returns Void Result on success
            core::Result<void> Disable();

            /// @brief Query the current supervision status (SWS_PHM_01142).
            /// @returns The global supervision status of this entity
            core::Result<GlobalSupervisionStatus> GetSupervisionStatus() const;

            /// @brief Query local supervision status per supervision type (SWS_PHM_01143).
            /// @param type Type of elementary supervision to query (0=alive, 1=deadline, 2=logical)
            /// @returns The local supervision status for the given type
            core::Result<LocalSupervisionStatus> GetLocalSupervisionStatus(
                std::uint32_t type) const;

            /// @brief Factory method to create a supervised entity (SWS_PHM_01130).
            /// @param specifier Instance specifier for the entity
            /// @returns A Result containing the entity on success
            static core::Result<SupervisedEntity> Create(
                const core::InstanceSpecifier &specifier);
        };
    }
}

#endif
