/// @file src/ara/phm/supervision_status.h
/// @brief PHM supervision status types per AUTOSAR AP SWS_PHM.
/// @details Provides GlobalSupervisionStatus, LocalSupervisionStatus,
///          HealthChannelExternalStatus, and RecoveryActionType enums.

#ifndef ARA_PHM_SUPERVISION_STATUS_H
#define ARA_PHM_SUPERVISION_STATUS_H

#include <cstdint>

namespace ara
{
    namespace phm
    {
        /// @brief Global supervision status (SWS_PHM_00110).
        /// @details Represents the aggregated supervision status across all
        ///          elementary supervisions of a supervised entity.
        enum class GlobalSupervisionStatus : std::uint32_t
        {
            kDeactivated = 0, ///< Supervision is deactivated
            kOk = 1,          ///< All elementary supervisions report OK
            kFailed = 2,      ///< At least one elementary supervision failed
            kExpired = 3,     ///< At least one elementary supervision expired
            kStopped = 4      ///< Supervision has been stopped
        };

        /// @brief Local supervision status (SWS_PHM_00111).
        /// @details Represents the status of a single elementary supervision.
        enum class LocalSupervisionStatus : std::uint32_t
        {
            kDeactivated = 0, ///< Supervision is deactivated
            kOk = 1,          ///< Supervision reports OK
            kFailed = 2,      ///< Supervision detection triggered
            kExpired = 3      ///< Supervision timeout expired
        };

        /// @brief External health channel status (SWS_PHM_00370).
        /// @details Externally visible status of a HealthChannel.
        enum class HealthChannelExternalStatus : std::uint32_t
        {
            kNormal = 0,  ///< Health channel reports normal operation
            kExpired = 1  ///< Health channel has expired
        };

        /// @brief Recovery action type discriminator (SWS_PHM_00112).
        /// @details Categorizes the kind of recovery action.
        enum class RecoveryActionType : std::uint32_t
        {
            kRestart = 0,                       ///< Restart the supervised entity
            kReset = 1,                         ///< Platform-level reset
            kFunctionGroupStateTransition = 2,  ///< Transition a function group
            kCustom = 3                         ///< Application-defined recovery
        };

    } // namespace phm
} // namespace ara

#endif // ARA_PHM_SUPERVISION_STATUS_H
