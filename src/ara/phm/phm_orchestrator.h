/// @file src/ara/phm/phm_orchestrator.h
/// @brief PHM orchestration daemon: aggregates supervision results and
///        dispatches recovery actions across all supervised entities.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PHM_PHM_ORCHESTRATOR_H
#define PHM_PHM_ORCHESTRATOR_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./recovery_action.h"
#include "./supervision_status.h"

namespace ara
{
    namespace phm
    {
        /// @brief Overall platform health state.
        enum class PlatformHealthState : uint8_t
        {
            kNormal = 0,   ///< All entities within tolerance.
            kDegraded = 1, ///< Some entities have expired or failed.
            kCritical = 2  ///< Threshold exceeded; platform reset imminent.
        };

        /// @brief Snapshot of an entity's health.
        struct EntityHealthSnapshot
        {
            std::string EntityName;
            GlobalSupervisionStatus Status{GlobalSupervisionStatus::kDeactivated};
            TypeOfSupervision LastFailedSupervision{
                TypeOfSupervision::AliveSupervision};
            uint32_t FailureCount{0};
            std::chrono::steady_clock::time_point LastFailureTime;
        };

        /// @brief Callback for platform-level health transitions.
        using PlatformHealthCallback =
            std::function<void(PlatformHealthState oldState,
                               PlatformHealthState newState)>;

        /// @brief Central PHM orchestrator coordinating all supervision and
        ///        recovery across the platform.
        class PhmOrchestrator
        {
        public:
            /// @param criticalThreshold Fraction of failed entities (0..1)
            ///        above which platform enters kCritical.
            explicit PhmOrchestrator(
                double degradedThreshold = 0.0,
                double criticalThreshold = 0.5);

            ~PhmOrchestrator() = default;

            /// @brief Register a supervised entity for tracking.
            core::Result<void> RegisterEntity(const std::string &name);

            /// @brief Unregister an entity.
            core::Result<void> UnregisterEntity(const std::string &name);

            /// @brief Report a supervision failure on an entity.
            core::Result<void> ReportSupervisionFailure(
                const std::string &entityName,
                TypeOfSupervision type);

            /// @brief Report that supervision recovered on an entity.
            core::Result<void> ReportSupervisionRecovery(
                const std::string &entityName);

            /// @brief Evaluate platform health from all entities.
            PlatformHealthState EvaluatePlatformHealth() const;

            /// @brief Get snapshot for a single entity.
            core::Result<EntityHealthSnapshot> GetEntityHealth(
                const std::string &entityName) const;

            /// @brief Get snapshots for all entities.
            std::vector<EntityHealthSnapshot> GetAllEntityHealth() const;

            /// @brief Set callback for platform health transitions.
            void SetPlatformHealthCallback(PlatformHealthCallback cb);

            /// @brief Reset all entity failure counts.
            void ResetAllCounters();

            /// @brief Get number of registered entities.
            size_t GetEntityCount() const;

        private:
            mutable std::mutex mMutex;
            double mDegradedThreshold;
            double mCriticalThreshold;
            std::map<std::string, EntityHealthSnapshot> mEntities;
            PlatformHealthState mCurrentState{PlatformHealthState::kNormal};
            PlatformHealthCallback mCallback;

            void UpdatePlatformState();
        };
    }
}

#endif
