/// @file src/ara/exec/cluster_monitor.h
/// @brief Cluster-level health monitor aggregating process statuses.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef EXEC_CLUSTER_MONITOR_H
#define EXEC_CLUSTER_MONITOR_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./execution_manager.h"

namespace ara
{
    namespace exec
    {
        /// @brief Overall health rating for a function-group cluster.
        enum class ClusterHealth : uint8_t
        {
            kHealthy = 0,
            kDegraded = 1,
            kUnhealthy = 2,
            kUnknown = 3
        };

        /// @brief Snapshot of cluster health at a point in time.
        struct ClusterSnapshot
        {
            std::string FunctionGroupName;
            ClusterHealth Health{ClusterHealth::kUnknown};
            uint32_t TotalProcesses{0};
            uint32_t RunningProcesses{0};
            uint32_t FailedProcesses{0};
            std::chrono::steady_clock::time_point Timestamp;
        };

        /// @brief Callback for cluster health changes.
        using ClusterHealthCallback =
            std::function<void(const std::string &functionGroup,
                               ClusterHealth oldHealth,
                               ClusterHealth newHealth)>;

        /// @brief Monitors health across all function-group clusters by
        ///        aggregating process statuses from ExecutionManager.
        class ClusterMonitor
        {
        public:
            /// @param em Reference to the ExecutionManager to query.
            /// @param degradedThreshold Fraction of failed processes (0..1)
            ///        above which the cluster is considered degraded.
            explicit ClusterMonitor(
                const ExecutionManager &em,
                double degradedThreshold = 0.0,
                double unhealthyThreshold = 0.5);

            ~ClusterMonitor() = default;

            /// @brief Refresh all cluster snapshots from the ExecutionManager.
            void Refresh();

            /// @brief Get the latest snapshot for a function group.
            core::Result<ClusterSnapshot> GetSnapshot(
                const std::string &functionGroup) const;

            /// @brief Get snapshots for all known function groups.
            std::vector<ClusterSnapshot> GetAllSnapshots() const;

            /// @brief Register a callback for health transitions.
            void SetHealthCallback(ClusterHealthCallback cb);

        private:
            const ExecutionManager &mEM;
            double mDegradedThreshold;
            double mUnhealthyThreshold;
            mutable std::mutex mMutex;
            std::map<std::string, ClusterSnapshot> mSnapshots;
            ClusterHealthCallback mCallback;

            ClusterHealth ComputeHealth(
                uint32_t total, uint32_t failed) const;
        };
    }
}

#endif
