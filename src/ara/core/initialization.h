/// @file src/ara/core/initialization.h
/// @brief Declarations for initialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#include "./result.h"

namespace ara
{
    namespace core
    {
        /// @brief Initialize the AUTOSAR Adaptive Runtime
        /// @returns A Result indicating success or containing an error
        /// @note This shall be the first ARA call in main()
        Result<void> Initialize();

        /// @brief Deinitialize the AUTOSAR Adaptive Runtime
        /// @returns A Result indicating success or containing an error
        /// @note This shall be the last ARA call in main()
        Result<void> Deinitialize();

        /// @brief Query whether the AUTOSAR Adaptive Runtime is initialized
        /// @returns True if Initialize() has been called and not yet deinitialized
        bool IsInitialized() noexcept;

        /// @brief Abnormal process termination handler
        /// @note This function does not return. It terminates the process.
        [[noreturn]] void Abort(const char *text) noexcept;
    }
}

#endif
