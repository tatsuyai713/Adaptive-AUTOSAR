/// @file src/ara/diag/diagnostic_session_manager.h
/// @brief Diagnostic Session Manager — UDS session lifecycle and S3 timer.
/// @details Manages the active UDS (ISO 14229-1) diagnostic session with:
///          - Session control (0x10): DefaultSession, ProgrammingSession,
///            ExtendedDiagnosticSession, SafetySystemDiagnosticSession
///          - S3 timer: automatic session timeout returning to DefaultSession
///          - Session state change callbacks
///          - DoIP / CAN session tracking
///
///          Reference: ISO 14229-1:2020 (UDS) §7.4.7 Session timing parameters
///          Reference: AUTOSAR_SWS_DiagnosticCommunicationManager
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_DIAGNOSTIC_SESSION_MANAGER_H
#define ARA_DIAG_DIAGNOSTIC_SESSION_MANAGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include "../core/result.h"
#include "./conversation.h"
#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        /// @brief UDS Session timing parameters (ISO 14229-1 §7.4.7).
        struct SessionTimingConfig
        {
            /// @brief S3 server timer: max time between requests in non-default session.
            /// @details When expired without any request, session returns to Default.
            ///          Typical value: 5000 ms (5 seconds).
            uint32_t s3TimerMs{5000};

            /// @brief P2 server: max time before response to a request.
            ///        Typical value: 50 ms.
            uint32_t p2ServerMs{50};

            /// @brief P2* server: extended response time for negative response 0x78 (RequestCorrectlyReceivedResponsePending).
            ///        Typical value: 5000 ms.
            uint32_t p2StarServerMs{5000};
        };

        /// @brief UDS response codes for Diagnostic Session Control (SID 0x10).
        enum class SessionControlResponse : uint8_t
        {
            kOk = 0x00,
            kSessionNotSupported = 0x12,       ///< SubFunctionNotSupported
            kConditionsNotCorrect = 0x22       ///< ConditionsNotCorrect
        };

        /// @brief Diagnostic Session Manager — manages UDS session state and S3 timer.
        /// @details Thread-safe session controller. The S3 timer runs in a background thread
        ///          and invokes the session timeout callback when expired.
        ///
        /// @example
        /// @code
        /// DiagnosticSessionManager mgr;
        /// mgr.SetSessionChangeCallback([](SessionControlType s) {
        ///     LOG(INFO) << "Session changed to: " << static_cast<int>(s);
        /// });
        /// mgr.Start();
        ///
        /// // On receiving UDS 0x10 request:
        /// mgr.RequestSessionChange(SessionControlType::kExtendedDiagnosticSession);
        ///
        /// // On every incoming UDS request (to reset S3 timer):
        /// mgr.ResetS3Timer();
        ///
        /// mgr.Stop();
        /// @endcode
        class DiagnosticSessionManager
        {
        public:
            /// @brief Callback type for session change notifications.
            using SessionChangeCallback = std::function<void(SessionControlType)>;

            /// @brief Callback type for S3 timer expiry notification.
            using S3TimeoutCallback = std::function<void()>;

            /// @brief Construct with default timing parameters.
            DiagnosticSessionManager();

            /// @brief Construct with custom timing parameters.
            explicit DiagnosticSessionManager(const SessionTimingConfig &config);

            ~DiagnosticSessionManager();

            // Not copyable
            DiagnosticSessionManager(const DiagnosticSessionManager &) = delete;
            DiagnosticSessionManager &operator=(const DiagnosticSessionManager &) = delete;

            /// @brief Start the S3 timer background thread.
            void Start();

            /// @brief Stop the S3 timer background thread.
            void Stop();

            /// @brief Get the current active diagnostic session.
            SessionControlType GetCurrentSession() const noexcept;

            /// @brief Request a session change (processes UDS 0x10 subfunction).
            /// @param requestedSession Target session.
            /// @returns Ok on success, or error code if session change is rejected.
            ara::core::Result<void> RequestSessionChange(SessionControlType requestedSession);

            /// @brief Reset the S3 server timer (call on every incoming UDS request).
            /// @details Prevents session timeout while tester is actively communicating.
            void ResetS3Timer();

            /// @brief Register a callback invoked when the session changes.
            void SetSessionChangeCallback(SessionChangeCallback callback);

            /// @brief Register a callback invoked when the S3 timer expires.
            /// @details Default behavior: return to DefaultSession automatically.
            ///          Override to implement custom expiry behavior.
            void SetS3TimeoutCallback(S3TimeoutCallback callback);

            /// @brief Get P2 server maximum response time (ms).
            uint32_t GetP2ServerMs() const noexcept;

            /// @brief Get P2* server extended response time (ms).
            uint32_t GetP2StarServerMs() const noexcept;

            /// @brief Get current session as string (for logging).
            static std::string SessionToString(SessionControlType session) noexcept;

        private:
            SessionTimingConfig mConfig;
            mutable std::mutex mMutex;
            SessionControlType mCurrentSession{SessionControlType::kDefaultSession};
            SessionChangeCallback mSessionChangeCallback;
            S3TimeoutCallback mS3TimeoutCallback;

            // S3 timer thread
            std::thread mS3Thread;
            std::atomic<bool> mRunning{false};
            std::atomic<bool> mS3ResetPending{false};
            std::chrono::steady_clock::time_point mLastRequestTime;

            void s3TimerLoop();
            void applySessionChange(SessionControlType newSession);
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_DIAGNOSTIC_SESSION_MANAGER_H
