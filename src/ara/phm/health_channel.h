/// @file src/ara/phm/health_channel.h
/// @brief Declarations for health channel.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef HEALTH_CHANNEL_H
#define HEALTH_CHANNEL_H

#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        /// @brief Health status reported via a HealthChannel
        enum class HealthStatus : uint32_t
        {
            kOk = 0,               ///< Health is normal
            kFailed = 1,           ///< Health check failed
            kExpired = 2,          ///< Health check expired (timeout)
            kDeactivated = 3       ///< Health channel is deactivated
        };

        /// @brief A class that represents a health reporting channel to PHM
        /// @note Per AUTOSAR AP, HealthChannel is used by application processes
        ///       to report their operational health status to Platform Health Management.
        class HealthChannel
        {
        private:
            const core::InstanceSpecifier mInstance;
            HealthStatus mLastReportedStatus;
            bool mOffered;

        public:
            /// @brief Constructor
            /// @param instance Adaptive application instance specifier for this health channel
            explicit HealthChannel(core::InstanceSpecifier instance);

            HealthChannel(HealthChannel &&other) noexcept;
            HealthChannel(const HealthChannel &) = delete;
            HealthChannel &operator=(const HealthChannel &) = delete;

            ~HealthChannel() noexcept = default;

            /// @brief Start offering health channel service
            /// @returns Error if already offered
            core::Result<void> Offer();

            /// @brief Stop offering health channel service
            void StopOffer();

            /// @brief Query whether this health channel is offered
            /// @returns True if Offer() was called and not stopped
            bool IsOffered() const noexcept;

            /// @brief Report a health status to the PHM functional cluster
            /// @param status The health status to report
            /// @returns Void Result if the health report was successful, error if not offered
            core::Result<void> ReportHealthStatus(HealthStatus status);

            /// @brief Get the last reported health status
            /// @returns The last health status that was reported
            HealthStatus GetLastReportedStatus() const noexcept;
        };
    }
}

#endif
