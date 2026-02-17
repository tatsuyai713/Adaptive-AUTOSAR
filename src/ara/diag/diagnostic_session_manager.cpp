/// @file src/ara/diag/diagnostic_session_manager.cpp
/// @brief Diagnostic Session Manager implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./diagnostic_session_manager.h"
#include <sstream>

namespace ara
{
    namespace diag
    {
        // -----------------------------------------------------------------------
        // Constructors / Destructor
        // -----------------------------------------------------------------------
        DiagnosticSessionManager::DiagnosticSessionManager()
            : mConfig{},
              mCurrentSession{SessionControlType::kDefaultSession},
              mLastRequestTime{std::chrono::steady_clock::now()}
        {
        }

        DiagnosticSessionManager::DiagnosticSessionManager(const SessionTimingConfig &config)
            : mConfig{config},
              mCurrentSession{SessionControlType::kDefaultSession},
              mLastRequestTime{std::chrono::steady_clock::now()}
        {
        }

        DiagnosticSessionManager::~DiagnosticSessionManager()
        {
            Stop();
        }

        // -----------------------------------------------------------------------
        // Start / Stop
        // -----------------------------------------------------------------------
        void DiagnosticSessionManager::Start()
        {
            if (mRunning.load())
            {
                return;
            }
            mRunning.store(true);
            mLastRequestTime = std::chrono::steady_clock::now();
            mS3Thread = std::thread(&DiagnosticSessionManager::s3TimerLoop, this);
        }

        void DiagnosticSessionManager::Stop()
        {
            if (!mRunning.load())
            {
                return;
            }
            mRunning.store(false);
            if (mS3Thread.joinable())
            {
                mS3Thread.join();
            }
        }

        // -----------------------------------------------------------------------
        // S3 Timer Loop — runs in background thread
        // -----------------------------------------------------------------------
        void DiagnosticSessionManager::s3TimerLoop()
        {
            const auto checkInterval = std::chrono::milliseconds(100);

            while (mRunning.load())
            {
                std::this_thread::sleep_for(checkInterval);

                // S3 timer only applies in non-default sessions
                SessionControlType current;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    current = mCurrentSession;
                }

                if (current == SessionControlType::kDefaultSession)
                {
                    // Reset last request time so S3 doesn't immediately trigger
                    // when entering a non-default session.
                    std::lock_guard<std::mutex> lock(mMutex);
                    mLastRequestTime = std::chrono::steady_clock::now();
                    continue;
                }

                // Check S3 timer expiry
                const auto now = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point lastReq;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    lastReq = mLastRequestTime;
                }

                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReq).count();

                if (elapsedMs >= static_cast<long long>(mConfig.s3TimerMs))
                {
                    // S3 timer expired — invoke callback or default to DefaultSession
                    S3TimeoutCallback cb;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        cb = mS3TimeoutCallback;
                    }

                    if (cb)
                    {
                        cb();
                    }
                    else
                    {
                        // Default: return to Default session
                        applySessionChange(SessionControlType::kDefaultSession);
                    }
                }
            }
        }

        // -----------------------------------------------------------------------
        // GetCurrentSession
        // -----------------------------------------------------------------------
        SessionControlType DiagnosticSessionManager::GetCurrentSession() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCurrentSession;
        }

        // -----------------------------------------------------------------------
        // RequestSessionChange
        // -----------------------------------------------------------------------
        ara::core::Result<void> DiagnosticSessionManager::RequestSessionChange(
            SessionControlType requestedSession)
        {
            // All standard sessions are accepted in this implementation.
            // Extend here for security-level or condition-based restrictions.
            applySessionChange(requestedSession);
            return {};
        }

        void DiagnosticSessionManager::applySessionChange(SessionControlType newSession)
        {
            SessionChangeCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mCurrentSession == newSession)
                {
                    return;
                }
                mCurrentSession = newSession;
                mLastRequestTime = std::chrono::steady_clock::now();
                cb = mSessionChangeCallback;
            }
            if (cb)
            {
                cb(newSession);
            }
        }

        // -----------------------------------------------------------------------
        // ResetS3Timer
        // -----------------------------------------------------------------------
        void DiagnosticSessionManager::ResetS3Timer()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mLastRequestTime = std::chrono::steady_clock::now();
        }

        // -----------------------------------------------------------------------
        // Callbacks
        // -----------------------------------------------------------------------
        void DiagnosticSessionManager::SetSessionChangeCallback(SessionChangeCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mSessionChangeCallback = std::move(callback);
        }

        void DiagnosticSessionManager::SetS3TimeoutCallback(S3TimeoutCallback callback)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mS3TimeoutCallback = std::move(callback);
        }

        // -----------------------------------------------------------------------
        // Timing getters
        // -----------------------------------------------------------------------
        uint32_t DiagnosticSessionManager::GetP2ServerMs() const noexcept
        {
            return mConfig.p2ServerMs;
        }

        uint32_t DiagnosticSessionManager::GetP2StarServerMs() const noexcept
        {
            return mConfig.p2StarServerMs;
        }

        // -----------------------------------------------------------------------
        // SessionToString
        // -----------------------------------------------------------------------
        std::string DiagnosticSessionManager::SessionToString(
            SessionControlType session) noexcept
        {
            switch (session)
            {
            case SessionControlType::kDefaultSession:
                return "DefaultSession(0x01)";
            case SessionControlType::kProgrammingSession:
                return "ProgrammingSession(0x02)";
            case SessionControlType::kExtendedDiagnosticSession:
                return "ExtendedDiagnosticSession(0x03)";
            case SessionControlType::kSafetySystemDiagnosticSession:
                return "SafetySystemDiagnosticSession(0x04)";
            default:
                return "UnknownSession";
            }
        }

    } // namespace diag
} // namespace ara
