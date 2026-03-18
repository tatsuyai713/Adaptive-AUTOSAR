/// @file src/ara/exec/application_client.h
/// @brief ApplicationClient for adaptive applications (SWS_EM_02100).
/// @details Provides application lifecycle reporting APIs that complement
///          ExecutionClient. ApplicationClient adds recovery-action requests
///          and application-level state transitions per SWS_EM_02100..02120.

#ifndef ARA_EXEC_APPLICATION_CLIENT_H
#define ARA_EXEC_APPLICATION_CLIENT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./exec_error_domain.h"

namespace ara
{
    namespace exec
    {
        /// @brief Recovery action requested by the platform (SWS_EM_02110).
        enum class ApplicationRecoveryAction : std::uint8_t
        {
            kNone = 0,       ///< No recovery action requested.
            kRestart = 1,    ///< Application should restart.
            kTerminate = 2   ///< Application should terminate.
        };

        /// @brief Application-level client for lifecycle management (SWS_EM_02100).
        ///
        /// Unlike ExecutionClient (which uses RPC to report states to the
        /// Execution Manager), ApplicationClient provides a lightweight,
        /// local API that an application uses to:
        ///   - Query the last requested recovery action.
        ///   - Acknowledge a recovery action.
        ///   - Report its own health/liveness.
        class ApplicationClient
        {
        public:
            /// @brief Callback type for recovery action requests.
            using RecoveryActionHandler = std::function<void(ApplicationRecoveryAction)>;

            /// @brief Construct an ApplicationClient.
            /// @param instanceSpecifier  Instance shortname-path for this application.
            explicit ApplicationClient(
                core::InstanceSpecifier instanceSpecifier);

            ~ApplicationClient() = default;
            ApplicationClient(const ApplicationClient &) = delete;
            ApplicationClient &operator=(const ApplicationClient &) = delete;

            /// @brief Get the last requested recovery action (SWS_EM_02110).
            /// @returns Current requested recovery action, or kNone.
            core::Result<ApplicationRecoveryAction> GetRecoveryAction() const;

            /// @brief Acknowledge that the application processed the recovery action.
            /// @details Resets the pending recovery action to kNone.
            /// @returns Void Result on success, error if no action was pending.
            core::Result<void> AcknowledgeRecoveryAction();

            /// @brief Report application liveness (SWS_EM_02120).
            /// @details Call periodically to indicate the application is alive
            ///          and functioning. Resets the internal aliveness counter.
            void ReportApplicationHealth() noexcept;

            /// @brief Set the handler invoked when a new recovery action is requested.
            /// @param handler Callback (may be nullptr to clear).
            void SetRecoveryActionHandler(RecoveryActionHandler handler);

            /// @brief Request a recovery action (platform-side API).
            /// @details Normally called by the Execution Manager or Health Monitor.
            /// @param action Recovery action to request.
            void RequestRecoveryAction(ApplicationRecoveryAction action);

            /// @brief Get the number of liveness reports since construction.
            std::uint64_t GetHealthReportCount() const noexcept;

            /// @brief Get the application instance specifier string.
            const std::string &GetInstanceId() const noexcept;

        private:
            core::InstanceSpecifier mInstanceSpecifier;
            std::string mInstanceId;
            mutable std::mutex mMutex;
            ApplicationRecoveryAction mPendingAction{ApplicationRecoveryAction::kNone};
            RecoveryActionHandler mHandler;
            std::atomic<std::uint64_t> mHealthReportCount{0U};
        };
    }
}

#endif
