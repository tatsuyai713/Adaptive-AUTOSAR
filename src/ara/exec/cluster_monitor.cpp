/// @file src/ara/exec/cluster_monitor.cpp
/// @brief Implementation of ClusterMonitor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./cluster_monitor.h"
#include "./exec_error_domain.h"

namespace ara
{
    namespace exec
    {
        ClusterMonitor::ClusterMonitor(
            const ExecutionManager &em,
            double degradedThreshold,
            double unhealthyThreshold)
            : mEM{em},
              mDegradedThreshold{degradedThreshold},
              mUnhealthyThreshold{unhealthyThreshold}
        {
        }

        void ClusterMonitor::Refresh()
        {
            auto statuses = mEM.GetAllProcessStatuses();

            // Group by function group.
            std::map<std::string, std::pair<uint32_t, uint32_t>> groups;
            for (const auto &s : statuses)
            {
                auto &fg = s.descriptor.functionGroup;
                auto &counts = groups[fg]; // {total, failed}
                ++counts.first;
                if (s.managedState == ManagedProcessState::kFailed)
                {
                    ++counts.second;
                }
            }

            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto &kv : groups)
            {
                ClusterSnapshot snap;
                snap.FunctionGroupName = kv.first;
                snap.TotalProcesses = kv.second.first;
                snap.FailedProcesses = kv.second.second;
                snap.RunningProcesses =
                    snap.TotalProcesses - snap.FailedProcesses;
                snap.Health = ComputeHealth(
                    snap.TotalProcesses, snap.FailedProcesses);
                snap.Timestamp = std::chrono::steady_clock::now();

                auto it = mSnapshots.find(kv.first);
                ClusterHealth oldHealth = ClusterHealth::kUnknown;
                if (it != mSnapshots.end())
                {
                    oldHealth = it->second.Health;
                }

                mSnapshots[kv.first] = snap;

                if (mCallback && oldHealth != snap.Health)
                {
                    mCallback(kv.first, oldHealth, snap.Health);
                }
            }
        }

        core::Result<ClusterSnapshot> ClusterMonitor::GetSnapshot(
            const std::string &functionGroup) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mSnapshots.find(functionGroup);
            if (it == mSnapshots.end())
            {
                return core::Result<ClusterSnapshot>::FromError(
                    MakeErrorCode(ExecErrc::kGeneralError));
            }
            return it->second;
        }

        std::vector<ClusterSnapshot> ClusterMonitor::GetAllSnapshots() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<ClusterSnapshot> result;
            result.reserve(mSnapshots.size());
            for (const auto &kv : mSnapshots)
            {
                result.push_back(kv.second);
            }
            return result;
        }

        void ClusterMonitor::SetHealthCallback(ClusterHealthCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(cb);
        }

        ClusterHealth ClusterMonitor::ComputeHealth(
            uint32_t total, uint32_t failed) const
        {
            if (total == 0) return ClusterHealth::kUnknown;
            double failRatio =
                static_cast<double>(failed) / static_cast<double>(total);
            if (failRatio >= mUnhealthyThreshold)
                return ClusterHealth::kUnhealthy;
            if (failRatio > mDegradedThreshold)
                return ClusterHealth::kDegraded;
            return ClusterHealth::kHealthy;
        }
    }
}
