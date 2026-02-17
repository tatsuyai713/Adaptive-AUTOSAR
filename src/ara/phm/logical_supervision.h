/// @file src/ara/phm/logical_supervision.h
/// @brief ara::phm::LogicalSupervision — Logical Supervision for PHM.
/// @details Monitors that checkpoints in a supervised entity are executed in
///          the expected order. Each checkpoint has a defined set of valid
///          successor checkpoints. Violating the expected order → kFailed.
///
///          Use case:
///          - A state machine with defined transitions (e.g., Init→Running→Shutdown)
///          - A processing pipeline with ordered stages
///
///          Reference: AUTOSAR_SWS_PlatformHealthManagement §7.4.5
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_PHM_LOGICAL_SUPERVISION_H
#define ARA_PHM_LOGICAL_SUPERVISION_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        /// @brief Logical checkpoint identifier type.
        using CheckpointId = uint32_t;

        /// @brief Logical Supervision status (SWS_PHM §7.4.5).
        enum class LogicalSupervisionStatus : uint8_t
        {
            kDeactivated = 0, ///< Supervision not started or stopped
            kOk = 1,          ///< Checkpoints received in expected order
            kFailed = 2,      ///< Unexpected checkpoint sequence detected
            kExpired = 3      ///< Consecutive failures exceeded threshold
        };

        /// @brief Transition definition for the logical supervision graph.
        struct LogicalTransition
        {
            CheckpointId from; ///< Source checkpoint ID
            CheckpointId to;   ///< Valid successor checkpoint ID
        };

        /// @brief Configuration for logical supervision.
        struct LogicalSupervisionConfig
        {
            /// @brief Checkpoint graph transitions (directed edges).
            /// @details Each entry defines a valid (from → to) transition.
            ///          A checkpoint may have multiple valid successors.
            std::vector<LogicalTransition> transitions;

            /// @brief Initial checkpoint ID (entry point).
            CheckpointId initialCheckpoint{0};

            /// @brief Consecutive failures before kExpired.
            uint32_t failedThreshold{3};

            /// @brief Consecutive valid sequences to recover kFailed → kOk.
            uint32_t passedThreshold{1};

            /// @brief Whether to allow the initial checkpoint to be re-entered from any state.
            ///        Useful for cyclical workflows (e.g., after Shutdown → reinit).
            bool allowReset{true};
        };

        /// @brief Logical Supervision monitor (SWS_PHM §7.4.5).
        /// @details Validates checkpoint execution order based on a directed graph
        ///          of valid transitions. Call ReportCheckpoint() in sequence.
        ///
        ///          Example (simple Init → Running → Shutdown cycle):
        ///          @code
        ///          constexpr CheckpointId CP_INIT     = 1;
        ///          constexpr CheckpointId CP_RUNNING  = 2;
        ///          constexpr CheckpointId CP_SHUTDOWN = 3;
        ///
        ///          LogicalSupervisionConfig cfg;
        ///          cfg.initialCheckpoint = CP_INIT;
        ///          cfg.transitions = {
        ///              {CP_INIT,     CP_RUNNING},
        ///              {CP_RUNNING,  CP_SHUTDOWN},
        ///              {CP_SHUTDOWN, CP_INIT},   // cycle
        ///          };
        ///
        ///          LogicalSupervision supervision(cfg);
        ///          supervision.SetStatusCallback([](LogicalSupervisionStatus s) { ... });
        ///          supervision.Start();
        ///
        ///          supervision.ReportCheckpoint(CP_INIT);     // OK
        ///          supervision.ReportCheckpoint(CP_RUNNING);  // OK
        ///          supervision.ReportCheckpoint(CP_SHUTDOWN); // OK
        ///          supervision.ReportCheckpoint(CP_INIT);     // OK (cycle allowed)
        ///          supervision.ReportCheckpoint(CP_SHUTDOWN); // FAILED (skip)
        ///          @endcode
        class LogicalSupervision
        {
        public:
            using StatusCallback = std::function<void(LogicalSupervisionStatus)>;

            explicit LogicalSupervision(const LogicalSupervisionConfig &config);
            ~LogicalSupervision() = default;

            LogicalSupervision(const LogicalSupervision &) = delete;
            LogicalSupervision &operator=(const LogicalSupervision &) = delete;

            /// @brief Start supervision (enables checkpoint reporting).
            void Start();

            /// @brief Stop supervision.
            void Stop();

            /// @brief Report a checkpoint to validate against the expected sequence.
            /// @param checkpointId The checkpoint being reported.
            /// @post Status updated based on whether the transition is valid.
            void ReportCheckpoint(CheckpointId checkpointId);

            /// @brief Get current supervision status.
            LogicalSupervisionStatus GetStatus() const noexcept;

            /// @brief Register a status change callback.
            void SetStatusCallback(StatusCallback callback);

            /// @brief Query valid successors for a given checkpoint.
            /// @param checkpointId Source checkpoint.
            /// @returns Set of valid successor checkpoint IDs.
            std::set<CheckpointId> GetValidSuccessors(CheckpointId checkpointId) const;

        private:
            LogicalSupervisionConfig mConfig;
            mutable std::mutex mMutex;

            LogicalSupervisionStatus mStatus{LogicalSupervisionStatus::kDeactivated};
            StatusCallback mStatusCallback;

            CheckpointId mCurrentCheckpoint;
            bool mInitialized{false};  // true after first checkpoint received

            uint32_t mFailedCount{0};
            uint32_t mPassedCount{0};

            // Precomputed adjacency: from → set of valid tos
            std::map<CheckpointId, std::set<CheckpointId>> mAdjacency;

            bool isValidTransition(CheckpointId from, CheckpointId to) const noexcept;
            void recordResult(bool passed);
            void updateStatus(LogicalSupervisionStatus newStatus);
            void buildAdjacency();
        };

    } // namespace phm
} // namespace ara

#endif // ARA_PHM_LOGICAL_SUPERVISION_H
