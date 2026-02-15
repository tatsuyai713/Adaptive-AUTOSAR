/// @file src/ara/core/initialization.cpp
/// @brief Implementation for initialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <cstdlib>
#include <iostream>
#include <atomic>
#include "./initialization.h"

namespace ara
{
    namespace core
    {
        static std::atomic<bool> sInitialized{false};

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

        [[noreturn]] void Abort(const char *text) noexcept
        {
            if (text != nullptr)
            {
                std::cerr << "ara::core::Abort: " << text << std::endl;
            }
            std::abort();
        }
    }
}
