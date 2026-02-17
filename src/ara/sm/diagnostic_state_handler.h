/// @file src/ara/sm/diagnostic_state_handler.h
/// @brief ara::sm::DiagnosticStateHandler — SM/Diag session state bridge.
/// @details Couples UDS diagnostic session transitions (from
///          ara::diag::DiagnosticSessionManager) to State Management function-
///          group states. This bridge is required in production ECUs where:
///          - Entering programming session must suppress normal communication
///            and disable watchdog monitoring.
///          - Extended diagnostic session requires relaxed timing constraints.
///          - Returning to default session must re-enable suppressed functions.
///
///          The handler also manages the timing constraints between sessions:
///          - Entering non-default session: notify SM to adjust function-group
///            states accordingly.
///          - Exiting non-default session (S3 timeout or explicit return):
///            restore previous function-group configuration.
///
///          Usage:
///          @code
///          DiagnosticSessionManager sessionMgr(spec, timingCfg);
///          DiagnosticStateHandler diagSmBridge(sessionMgr);
///
///          // Register handlers for specific session transitions
///          diagSmBridge.SetSessionEntryHandler(
///              SessionControlType::kProgrammingSession,
///              [&]() {
///                  commCtrl.DisableTxAndRx();
///                  watchdog.Stop();
///              });
///
///          diagSmBridge.SetSessionExitHandler(
///              SessionControlType::kProgrammingSession,
///              [&]() {
///                  commCtrl.EnableTxAndRx();
///                  watchdog.Start();
///              });
///
///          diagSmBridge.Start();
///          @endcode
///
///          Reference: AUTOSAR SWS_SM §7.5 (Diagnostic interface), SWS_Diag §7.3
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_SM_DIAGNOSTIC_STATE_HANDLER_H
#define ARA_SM_DIAGNOSTIC_STATE_HANDLER_H

#include <functional>
#include <map>
#include <mutex>
#include "../core/result.h"
#include "../diag/diagnostic_session_manager.h"

namespace ara
{
    namespace sm
    {
        /// @brief Bridges UDS diagnostic session changes to SM state transitions.
        class DiagnosticStateHandler
        {
        public:
            /// @brief Handler invoked when entering a specific diagnostic session.
            using SessionEntryHandler = std::function<void()>;

            /// @brief Handler invoked when exiting a specific diagnostic session
            ///        (either by explicit request or by S3 timeout).
            using SessionExitHandler = std::function<void()>;

            /// @brief Construct the bridge and attach it to the session manager.
            /// @param sessionManager Reference to the DiagnosticSessionManager.
            ///        The caller must ensure sessionManager lifetime ≥ this object.
            explicit DiagnosticStateHandler(
                ara::diag::DiagnosticSessionManager &sessionManager) noexcept;

            ~DiagnosticStateHandler();

            // Non-copyable
            DiagnosticStateHandler(const DiagnosticStateHandler &) = delete;
            DiagnosticStateHandler &operator=(const DiagnosticStateHandler &) = delete;

            /// @brief Register a callback for entering a specific session.
            /// @param session  Session type to hook.
            /// @param handler  Callback invoked on session entry.
            void SetSessionEntryHandler(
                ara::diag::SessionControlType session,
                SessionEntryHandler handler);

            /// @brief Register a callback for exiting a specific session.
            /// @param session  Session type to hook.
            /// @param handler  Callback invoked on session exit (including S3 timeout).
            void SetSessionExitHandler(
                ara::diag::SessionControlType session,
                SessionExitHandler handler);

            /// @brief Unregister both entry and exit handlers for a session.
            void ClearHandlers(ara::diag::SessionControlType session);

            /// @brief Activate the bridge (registers callbacks with sessionManager).
            void Start();

            /// @brief Deactivate the bridge (unregisters callbacks).
            void Stop();

            /// @brief Get the current UDS session as seen by this bridge.
            ara::diag::SessionControlType GetCurrentSession() const noexcept;

        private:
            ara::diag::DiagnosticSessionManager &mSessionManager;
            mutable std::mutex mMutex;

            std::map<ara::diag::SessionControlType, SessionEntryHandler> mEntryHandlers;
            std::map<ara::diag::SessionControlType, SessionExitHandler>  mExitHandlers;

            ara::diag::SessionControlType mCurrentSession{
                ara::diag::SessionControlType::kDefaultSession};
            ara::diag::SessionControlType mPreviousSession{
                ara::diag::SessionControlType::kDefaultSession};

            bool mActive{false};

            void onSessionChanged(ara::diag::SessionControlType newSession);
            void onS3Timeout();
        };

    } // namespace sm
} // namespace ara

#endif // ARA_SM_DIAGNOSTIC_STATE_HANDLER_H
