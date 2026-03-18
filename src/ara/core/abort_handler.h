/// @file src/ara/core/abort_handler.h
/// @brief OS-level abort/signal handler integration for ara::core.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CORE_ABORT_HANDLER_H
#define CORE_ABORT_HANDLER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace ara
{
    namespace core
    {
        /// @brief Type of abort/signal that triggered the handler.
        enum class AbortReason : uint8_t
        {
            kUserAbort = 0,        ///< std::abort() or SIGABRT.
            kSegmentationFault = 1,///< SIGSEGV.
            kFloatingPoint = 2,    ///< SIGFPE.
            kIllegalInstruction = 3,///< SIGILL.
            kTermination = 4,      ///< SIGTERM.
            kInterrupt = 5,        ///< SIGINT.
            kBusError = 6,         ///< SIGBUS.
            kUnknown = 255
        };

        /// @brief Information passed to the abort callback.
        struct AbortInfo
        {
            AbortReason Reason{AbortReason::kUnknown};
            int SignalNumber{0};
            std::string Message;
        };

        /// @brief User-registered callback invoked before process termination.
        using AbortCallback = std::function<void(const AbortInfo &)>;

        /// @brief Manages OS signal handlers and abort callbacks.
        /// @details Thread-safe singleton. Installs signal handlers that
        ///          invoke registered callbacks before termination.
        class AbortHandler
        {
        public:
            /// @brief Get the singleton instance.
            static AbortHandler &Instance();

            AbortHandler(const AbortHandler &) = delete;
            AbortHandler &operator=(const AbortHandler &) = delete;

            /// @brief Register a callback to be invoked on abort/signal.
            /// @returns Index of the registered callback.
            size_t RegisterCallback(AbortCallback cb);

            /// @brief Remove a previously registered callback by index.
            void UnregisterCallback(size_t index);

            /// @brief Install OS signal handlers for common fatal signals.
            void InstallSignalHandlers();

            /// @brief Restore default OS signal handlers.
            void RestoreDefaultHandlers();

            /// @brief Manually trigger the abort sequence with a reason.
            void TriggerAbort(AbortReason reason,
                              const std::string &message = "");

            /// @brief Get the number of registered callbacks.
            size_t GetCallbackCount() const;

            /// @brief Check if signal handlers have been installed.
            bool HandlersInstalled() const;

        private:
            AbortHandler() = default;

            mutable std::mutex mMutex;
            std::vector<AbortCallback> mCallbacks;
            bool mInstalled{false};

            void InvokeCallbacks(const AbortInfo &info);

            /// @brief Static signal handler forwarding to the instance.
            static void SignalDispatcher(int signum);
        };
    }
}

#endif
