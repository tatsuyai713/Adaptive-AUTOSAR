/// @file src/ara/ucm/dependency_checker.h
/// @brief Software package dependency checking for UCM.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// `DependencyChecker` maintains a directed dependency graph among software
/// clusters and validates that all constraints are satisfied before a
/// package activation is permitted.

#ifndef DEPENDENCY_CHECKER_H
#define DEPENDENCY_CHECKER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief A single dependency constraint.
        struct DependencyConstraint
        {
            /// The cluster that has the dependency.
            std::string SourceCluster;
            /// The cluster that is required.
            std::string RequiredCluster;
            /// Minimum required version (dotted, e.g., "1.2.0").
            std::string MinVersion;
            /// Maximum allowed version (empty = no upper bound).
            std::string MaxVersion;
        };

        /// @brief Result of a dependency check for one constraint.
        struct DependencyCheckResult
        {
            DependencyConstraint Constraint;
            bool Satisfied{false};
            /// Present version of the required cluster (empty if not found).
            std::string PresentVersion;
            std::string Reason;
        };

        /// @brief Software cluster dependency checker.
        ///
        /// Manages a set of dependency constraints (directional edges in a
        /// dependency graph) and validates them against a given map of
        /// cluster -> active-version.
        class DependencyChecker
        {
        public:
            DependencyChecker() noexcept = default;

            /// @brief Register a dependency constraint.
            /// @param constraint  The dependency to add.
            /// @returns Void on success, error on invalid input.
            core::Result<void> AddDependency(
                const DependencyConstraint &constraint);

            /// @brief Remove all dependencies for a source cluster.
            /// @param sourceCluster  Cluster whose dependencies to remove.
            /// @returns Number of constraints removed.
            core::Result<std::size_t> RemoveDependenciesForCluster(
                const std::string &sourceCluster);

            /// @brief Get all constraints registered for a source cluster.
            std::vector<DependencyConstraint> GetDependenciesForCluster(
                const std::string &sourceCluster) const;

            /// @brief Get all registered constraints.
            std::vector<DependencyConstraint> GetAllDependencies() const;

            /// @brief Check whether all dependencies for a cluster are met.
            ///
            /// @param sourceCluster    Cluster to check.
            /// @param clusterVersions  Map of cluster-name -> active-version.
            /// @returns Void if all OK, or a descriptive error.
            core::Result<void> CheckDependencies(
                const std::string &sourceCluster,
                const std::unordered_map<std::string, std::string> &clusterVersions) const;

            /// @brief Check all registered dependencies for all clusters.
            ///
            /// @param clusterVersions  Map of cluster-name -> active-version.
            /// @returns Detailed per-constraint results.
            std::vector<DependencyCheckResult> CheckAllDependencies(
                const std::unordered_map<std::string, std::string> &clusterVersions) const;

            /// @brief Save dependency graph to file.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load dependency graph from file.
            core::Result<void> LoadFromFile(const std::string &filePath);

            /// @brief Clear all constraints.
            void Clear();

        private:
            mutable std::mutex mMutex;
            std::vector<DependencyConstraint> mConstraints;

            /// Compare two dotted version strings.
            /// @returns -1 if a < b, 0 if a == b, 1 if a > b.
            static int CompareVersions(
                const std::string &a,
                const std::string &b);
        };
    }
}

#endif
