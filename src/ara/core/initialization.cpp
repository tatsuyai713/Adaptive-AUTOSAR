/// @file src/ara/core/initialization.cpp
/// @brief Implementation for initialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <cstdlib>
#include <iostream>
#include <atomic>
#include <mutex>
#include "./initialization.h"

namespace ara
{
    namespace core
    {
        static std::atomic<bool> sInitialized{false};
        static AbortHandler sAbortHandler;
        static std::mutex sAbortHandlerMutex;

        Result<void> Initialize()
        {
            bool _expected{false};
            if (sInitialized.compare_exchange_strong(_expected, true))
            {
                // Platform-level initialization would go here
                return Result<void>::FromValue();
            }
            else
            {
                // Already initialized - still return success per AP spec
                return Result<void>::FromValue();
            }
        }

        Result<void> Initialize(const InitOptions &options)
        {
            // Store options for platform use, then delegate
            (void)options; // reserved for future use
            return Initialize();
        }

        Result<void> Deinitialize()
        {
            bool _expected{true};
            if (sInitialized.compare_exchange_strong(_expected, false))
            {
                // Platform-level cleanup would go here
                return Result<void>::FromValue();
            }
            else
            {
                // Not initialized - still return success per AP spec
                return Result<void>::FromValue();
            }
        }

        bool IsInitialized() noexcept
        {
            return sInitialized.load();
        }

        AbortHandler SetAbortHandler(AbortHandler handler) noexcept
        {
            const std::lock_guard<std::mutex> lock{sAbortHandlerMutex};
            AbortHandler previous = std::move(sAbortHandler);
            sAbortHandler = std::move(handler);
            return previous;
        }

        [[noreturn]] void Abort(const char *text) noexcept
        {
            {
                const std::lock_guard<std::mutex> lock{sAbortHandlerMutex};
                if (sAbortHandler)
                {
                    sAbortHandler(text);
                }
            }

            if (text != nullptr)
            {
                std::cerr << "ara::core::Abort: " << text << std::endl;
            }
            std::abort();
        }
    }
}
