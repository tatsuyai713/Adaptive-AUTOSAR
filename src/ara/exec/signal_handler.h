/// @file src/ara/exec/signal_handler.h
/// @brief Declarations for signal handler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ara
{
    namespace exec
    {
        /// @brief Signal handler for graceful shutdown of adaptive applications.
        /// @note In AUTOSAR AP, Execution Management sends SIGTERM to request
        ///       graceful shutdown. This class provides a portable way to handle
        ///       termination signals (SIGTERM and SIGINT).
        class SignalHandler
        {
        private:
            static std::atomic<bool> mTerminationRequested;
            static std::mutex mMutex;
            static std::condition_variable mCondVar;

            static void handleSignal(int signal);

        public:
            /// @brief Register signal handlers for SIGTERM and SIGINT
            /// @note This should be called once at application startup
            static void Register();

            /// @brief Block until a termination signal is received
            /// @note Returns immediately if termination was already requested
            static void WaitForTermination();

            /// @brief Check whether termination has been requested
            /// @returns True if SIGTERM or SIGINT has been received
            static bool IsTerminationRequested() noexcept;

            /// @brief Reset the termination flag (for testing purposes)
            static void Reset() noexcept;
        };
    }
}

#endif
