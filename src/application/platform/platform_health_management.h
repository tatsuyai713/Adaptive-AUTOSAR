/// @file src/application/platform/platform_health_management.h
/// @brief Declarations for platform health management.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PLATFORM_HEALTH_MANAGEMENT_H
#define PLATFORM_HEALTH_MANAGEMENT_H

#include <set>
#include "../../arxml/arxml_reader.h"
#include "../../ara/exec/helper/modelled_process.h"
#include "../../ara/phm/checkpoint_communicator.h"
#include "../../ara/phm/supervisors/alive_supervision.h"
#include "../../ara/phm/supervisors/deadline_supervision.h"
#include "../../ara/phm/supervisors/global_supervision.h"
#include "../helper/log_recovery_action.h"

namespace application
{
    namespace platform
    {
        /// @brief Platform health managment modelled process
        class PlatformHealthManagement : public ara::exec::helper::ModelledProcess
        {
        private:
            static const std::string cAppId;
            const ara::exec::FunctionGroup cFunctionGroup;

            ara::phm::CheckpointCommunicator *const mCheckpointCommunicator;
            ara::phm::supervisors::AliveSupervision *mAliveSupervision;
            ara::phm::supervisors::DeadlineSupervision *mDeadlineSupervision;
            ara::phm::supervisors::GlobalSupervision *mGlobalSupervision;
            helper::LogRecoveryAction mRecoveryAction;
            std::map<uint32_t, std::function<void()>> mReportDelegates;

            /// @brief Extracts checkpoint ID from a checkpoint ARXML snippet.
            /// @param content XML fragment that contains `CHECKPOINT-ID`.
            /// @returns Parsed checkpoint identifier.
            static uint32_t getCheckpointId(const std::string &content);
            /// @brief Collects all configured checkpoint IDs from PHM ARXML.
            /// @param reader Parsed ARXML reader.
            /// @param checkpoints Output set for discovered checkpoint IDs.
            static void fillCheckpoints(
                const arxml::ArxmlReader &reader,
                std::set<uint32_t> &checkpoints);

            /// @brief Dispatches one checkpoint report to its mapped supervision callback.
            /// @param checkpoint Checkpoint ID reported by application side.
            void onReportCheckpoint(uint32_t checkpoint);
            /// @brief Registers one checkpoint-to-callback mapping.
            /// @param checkpointIdStr Checkpoint ID string from ARXML references.
            /// @param checkpoints Valid checkpoint set from configuration.
            /// @param delegate Callback invoked when the checkpoint is reported.
            /// @returns `true` when registration succeeded.
            bool tryAddReportDelegate(
                std::string checkpointIdStr,
                const std::set<uint32_t> &checkpoints,
                std::function<void()> &&delegate);
            /// @brief Builds and configures `AliveSupervision` from ARXML.
            /// @param reader Parsed PHM ARXML.
            /// @param checkpoints Valid checkpoint set from configuration.
            void configureAliveSupervision(
                const arxml::ArxmlReader &reader,
                const std::set<uint32_t> &checkpoints);
            /// @brief Builds and configures `DeadlineSupervision` from ARXML.
            /// @param reader Parsed PHM ARXML.
            /// @param checkpoints Valid checkpoint set from configuration.
            void configureDeadlineSupervision(
                const arxml::ArxmlReader &reader,
                const std::set<uint32_t> &checkpoints);
            /// @brief Handles global supervision status changes and triggers recovery.
            /// @param update Global supervision update.
            void onGlobalStatusChanged(
                ara::phm::supervisors::SupervisionUpdate update);

        protected:
            /// @brief Main execution loop for platform PHM process.
            /// @param cancellationToken Cooperative stop token.
            /// @param arguments Process argument map.
            /// @returns Process exit code.
            int Main(
                const std::atomic_bool *cancellationToken,
                const std::map<std::string, std::string> &arguments) override;

        public:
            /// @brief Constructor
            /// @param poller Global poller for network communication
            /// @param checkpointCommunicator Medium to communicate the supervision checkpoints
            /// @param functionGroup Function group name monitored by the PHM
            PlatformHealthManagement(
                AsyncBsdSocketLib::Poller *poller,
                ara::phm::CheckpointCommunicator *checkpointCommunicator,
                std::string functionGroup);

            /// @brief Destructor.
            ~PlatformHealthManagement() override;
        };
    }
}

#endif
