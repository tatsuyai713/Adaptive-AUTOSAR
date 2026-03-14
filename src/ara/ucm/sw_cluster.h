/// @file src/ara/ucm/sw_cluster.h
/// @brief Software cluster and package data models per AUTOSAR AP SWS_UCM.
/// @details Provides SwClusterInfoType, SwPackageInfoType, PackageManagerStatusType,
///          and TransferIdType as required by the UCM specification.

#ifndef ARA_UCM_SW_CLUSTER_H
#define ARA_UCM_SW_CLUSTER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ara
{
    namespace ucm
    {
        /// @brief State of a software cluster (SWS_UCM_00022).
        enum class SwClusterStateType : std::uint32_t
        {
            kPresent = 0,  ///< Cluster is installed and active
            kAdded = 1,    ///< Cluster was newly added (pending activation)
            kUpdated = 2,  ///< Cluster was updated (pending activation)
            kRemoved = 3   ///< Cluster was removed (pending cleanup)
        };

        /// @brief Software cluster information (SWS_UCM_00023).
        struct SwClusterInfoType
        {
            std::string name;                   ///< Cluster short name
            std::string version;                ///< Version string (e.g. "1.2.0")
            SwClusterStateType state{SwClusterStateType::kPresent};
            std::string description;            ///< Human-readable description
            std::vector<std::string> processes;  ///< Process names belonging to this cluster
        };

        /// @brief Action type for a software package (SWS_UCM_00024).
        enum class SwPackageActionType : std::uint32_t
        {
            kInstall = 0,  ///< Fresh installation
            kUpdate = 1,   ///< Update existing cluster
            kRemove = 2    ///< Remove existing cluster
        };

        /// @brief Software package information (SWS_UCM_00025).
        struct SwPackageInfoType
        {
            std::string name;                    ///< Package short name
            std::string version;                 ///< Target version
            SwPackageActionType action{SwPackageActionType::kInstall};
            std::uint64_t sizeBytes{0};          ///< Package size in bytes
            std::string mimeType;                ///< MIME type (e.g. "application/x-autosar-swp")
            std::string clusterName;             ///< Target cluster name
        };

        /// @brief Overall UCM status (SWS_UCM_00026).
        enum class PackageManagerStatusType : std::uint32_t
        {
            kIdle = 0,         ///< No active operation
            kReady = 1,        ///< Package transferred, ready to process
            kBusy = 2,         ///< Processing in progress
            kActivating = 3,   ///< Activation in progress
            kActivated = 4,    ///< Activation completed successfully
            kRollingBack = 5,  ///< Rollback in progress
            kRolledBack = 6,   ///< Rollback completed
            kCleaningUp = 7    ///< Cleanup in progress
        };

        /// @brief Unique identifier for a transfer session (SWS_UCM_00027).
        using TransferIdType = std::uint64_t;

    } // namespace ucm
} // namespace ara

#endif // ARA_UCM_SW_CLUSTER_H
