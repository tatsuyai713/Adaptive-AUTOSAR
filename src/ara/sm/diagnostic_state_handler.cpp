/// @file src/ara/sm/diagnostic_state_handler.cpp
/// @brief Implementation for ara::sm::DiagnosticStateHandler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./diagnostic_state_handler.h"

namespace ara
{
    namespace sm
    {
        DiagnosticStateHandler::DiagnosticStateHandler(
            ara::diag::DiagnosticSessionManager &sessionManager) noexcept
            : mSessionManager{sessionManager}
        {
        }

        DiagnosticStateHandler::~DiagnosticStateHandler()
        {
            Stop();
        }

        // -----------------------------------------------------------------------
        // Handler registration
        // -----------------------------------------------------------------------
        void DiagnosticStateHandler::SetSessionEntryHandler(
            ara::diag::SessionControlType session,
            SessionEntryHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntryHandlers[session] = std::move(handler);
        }

        void DiagnosticStateHandler::SetSessionExitHandler(
            ara::diag::SessionControlType session,
            SessionExitHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mExitHandlers[session] = std::move(handler);
        }

        void DiagnosticStateHandler::ClearHandlers(ara::diag::SessionControlType session)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntryHandlers.erase(session);
            mExitHandlers.erase(session);
        }

        // -----------------------------------------------------------------------
        // Start / Stop
        // -----------------------------------------------------------------------
        void DiagnosticStateHandler::Start()
        {
            if (mActive)
            {
                return;
            }

            // Snapshot current session before registering callbacks
            mCurrentSession = mSessionManager.GetCurrentSession();
            mPreviousSession = mCurrentSession;

            // Register session-change callback
            mSessionManager.SetSessionChangeCallback(
                [this](ara::diag::SessionControlType newSession) {
                    onSessionChanged(newSession);
                });

            // Register S3 timeout callback
            mSessionManager.SetS3TimeoutCallback(
                [this]() {
                    onS3Timeout();
                });

            mActive = true;
        }

        void DiagnosticStateHandler::Stop()
        {
            if (!mActive)
            {
                return;
            }

            // Clear callbacks from session manager
            mSessionManager.SetSessionChangeCallback(nullptr);
            mSessionManager.SetS3TimeoutCallback(nullptr);

            mActive = false;
        }

        // -----------------------------------------------------------------------
        // Accessors
        // -----------------------------------------------------------------------
        ara::diag::SessionControlType
        DiagnosticStateHandler::GetCurrentSession() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCurrentSession;
        }

        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------
        void DiagnosticStateHandler::onSessionChanged(
            ara::diag::SessionControlType newSession)
        {
            SessionExitHandler exitHandler;
            SessionEntryHandler entryHandler;
            ara::diag::SessionControlType prevSession;

            {
                std::lock_guard<std::mutex> lock(mMutex);
                prevSession = mCurrentSession;

                // Find exit handler for old session
                auto exitIt = mExitHandlers.find(prevSession);
                if (exitIt != mExitHandlers.end())
                {
                    exitHandler = exitIt->second;
                }

                // Find entry handler for new session
                auto entryIt = mEntryHandlers.find(newSession);
                if (entryIt != mEntryHandlers.end())
                {
                    entryHandler = entryIt->second;
                }

                mPreviousSession = mCurrentSession;
                mCurrentSession = newSession;
            }

            // Invoke callbacks outside lock
            if (exitHandler)
            {
                exitHandler();
            }
            if (entryHandler)
            {
                entryHandler();
            }
        }

        void DiagnosticStateHandler::onS3Timeout()
        {
            // S3 timeout causes session to return to Default
            // The session manager will call onSessionChanged with kDefaultSession,
            // but we also invoke the exit handler for the current non-default session
            // here to allow immediate reaction (before the formal session change).
            SessionExitHandler exitHandler;
            ara::diag::SessionControlType prevSession;

            {
                std::lock_guard<std::mutex> lock(mMutex);
                prevSession = mCurrentSession;
                if (prevSession == ara::diag::SessionControlType::kDefaultSession)
                {
                    return; // Already in default; nothing to do
                }

                auto exitIt = mExitHandlers.find(prevSession);
                if (exitIt != mExitHandlers.end())
                {
                    exitHandler = exitIt->second;
                }
            }

            if (exitHandler)
            {
                exitHandler();
            }
        }

    } // namespace sm
} // namespace ara
