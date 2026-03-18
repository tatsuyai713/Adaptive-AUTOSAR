/// @file src/ara/exec/manifest_loader.h
/// @brief ARXML-driven execution manifest loader for process descriptors.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef EXEC_MANIFEST_LOADER_H
#define EXEC_MANIFEST_LOADER_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./execution_manager.h"

namespace ara
{
    namespace exec
    {
        /// @brief Extended manifest entry with resource limits and dependencies.
        struct ManifestProcessEntry
        {
            /// @brief Core descriptor that can feed ExecutionManager::RegisterProcess.
            ProcessDescriptor Descriptor;

            /// @brief Environment variables to set before launching the process.
            std::map<std::string, std::string> EnvironmentVariables;

            /// @brief CPU quota in percentage (0 = unlimited, cgroup-like).
            uint32_t CpuQuotaPercent{0};

            /// @brief Memory limit in bytes (0 = unlimited).
            uint64_t MemoryLimitBytes{0};

            /// @brief Names of other processes this process depends on.
            std::vector<std::string> Dependencies;

            /// @brief Numeric priority (lower = starts first).
            uint32_t StartupPriority{100};
        };

        /// @brief Result of loading an execution manifest.
        struct ManifestLoadResult
        {
            std::vector<ManifestProcessEntry> Entries;
            std::vector<std::string> Warnings;
            bool Success{false};
        };

        /// @brief Load process descriptors from an ARXML execution manifest.
        class ExecutionManifestLoader
        {
        public:
            ExecutionManifestLoader() = default;
            ~ExecutionManifestLoader() = default;

            /// @brief Load manifest from an ARXML file path.
            ManifestLoadResult LoadFromFile(const std::string &filePath) const;

            /// @brief Load manifest from XML string content.
            ManifestLoadResult LoadFromString(
                const std::string &xmlContent) const;

            /// @brief Validate entries for consistency (circular deps, etc.).
            std::vector<std::string> ValidateEntries(
                const std::vector<ManifestProcessEntry> &entries) const;

            /// @brief Sort entries by startup order (dependencies + priority).
            std::vector<ManifestProcessEntry> SortByStartupOrder(
                std::vector<ManifestProcessEntry> entries) const;

        private:
            ManifestProcessEntry ParseProcessNode(
                const std::string &nodeName,
                const std::map<std::string, std::string> &attributes) const;
        };
    }
}

#endif
