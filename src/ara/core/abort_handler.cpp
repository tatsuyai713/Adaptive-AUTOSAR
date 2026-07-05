/// @file src/ara/core/abort_handler.cpp
/// @brief Implementation of AbortHandler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./abort_handler.h"
#include <cstdlib>
#include <csignal>

namespace ara
{
    namespace core
    {
        AbortHandler &AbortHandler::Instance()
        {
            static AbortHandler instance;
            return instance;
        }

        size_t AbortHandler::RegisterCallback(AbortCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallbacks.push_back(std::move(cb));
            return mCallbacks.size() - 1;
        }

        void AbortHandler::UnregisterCallback(size_t index)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (index < mCallbacks.size())
            {
                mCallbacks[index] = nullptr;
            }
        }

        void AbortHandler::InstallSignalHandlers()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mInstalled) return;

            std::signal(SIGABRT, &AbortHandler::SignalDispatcher);
            std::signal(SIGSEGV, &AbortHandler::SignalDispatcher);
            std::signal(SIGFPE, &AbortHandler::SignalDispatcher);
            std::signal(SIGILL, &AbortHandler::SignalDispatcher);
            std::signal(SIGTERM, &AbortHandler::SignalDispatcher);
            std::signal(SIGINT, &AbortHandler::SignalDispatcher);
#ifdef SIGBUS
            std::signal(SIGBUS, &AbortHandler::SignalDispatcher);
#endif
            mInstalled = true;
        }

        void AbortHandler::RestoreDefaultHandlers()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mInstalled) return;

            std::signal(SIGABRT, SIG_DFL);
            std::signal(SIGSEGV, SIG_DFL);
            std::signal(SIGFPE, SIG_DFL);
            std::signal(SIGILL, SIG_DFL);
            std::signal(SIGTERM, SIG_DFL);
            std::signal(SIGINT, SIG_DFL);
#ifdef SIGBUS
            std::signal(SIGBUS, SIG_DFL);
#endif
            mInstalled = false;
        }

        void AbortHandler::TriggerAbort(
            AbortReason reason, const std::string &message)
        {
            AbortInfo info;
            info.Reason = reason;
            info.Message = message;
            InvokeCallbacks(info);
        }

        size_t AbortHandler::GetCallbackCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            size_t count = 0;
            for (const auto &cb : mCallbacks)
            {
                if (cb) ++count;
            }
            return count;
        }

        bool AbortHandler::HandlersInstalled() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mInstalled;
        }

        void AbortHandler::InvokeCallbacks(const AbortInfo &info)
        {
            std::vector<AbortCallback> callbacks;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                callbacks = mCallbacks;
            }

            for (const auto &cb : callbacks)
            {
                if (cb)
                {
                    cb(info);
                }
            }
        }

        void AbortHandler::SignalDispatcher(int signum)
        {
            std::_Exit(128 + signum);
        }
    }
}
