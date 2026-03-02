/// @file src/ara/core/initialization.h
/// @brief Declarations for initialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#include <functional>
#include "./result.h"

namespace ara
{
    namespace core
    {
        /// @brief Options for runtime initialization (SWS_CORE_10003).
        /// @details Allows platform-level configuration at Initialize time.
        struct InitOptions
        {
            /// @brief Optional log level override for the runtime (0 = default).
            uint8_t logLevel{0};

            /// @brief Request strict mode for ARXML parsing on startup.
            bool strictArxmlValidation{false};
        };

        /// @brief Abort handler function type (SWS_CORE_10050).
        using AbortHandler = std::function<void(const char *)>;

        /// @brief Initialize the AUTOSAR Adaptive Runtime (SWS_CORE_10001)
        /// @returns A Result indicating success or containing an error
        /// @note This shall be the first ARA call in main()
        Result<void> Initialize();

        /// @brief Initialize the AUTOSAR Adaptive Runtime with options (SWS_CORE_10003)
        /// @param options Configuration options for the runtime
        /// @returns A Result indicating success or containing an error
        Result<void> Initialize(const InitOptions &options);

        /// @brief Deinitialize the AUTOSAR Adaptive Runtime (SWS_CORE_10002)
        /// @returns A Result indicating success or containing an error
        /// @note This shall be the last ARA call in main()
        Result<void> Deinitialize();

        /// @brief Query whether the AUTOSAR Adaptive Runtime is initialized
        /// @returns True if Initialize() has been called and not yet deinitialized
        bool IsInitialized() noexcept;

        /// @brief Set a custom abort handler (SWS_CORE_10050)
        /// @param handler Function to be called by Abort(). If nullptr, resets to default.
        /// @returns The previously installed handler (or empty function if none).
        AbortHandler SetAbortHandler(AbortHandler handler) noexcept;

        /// @brief Abnormal process termination handler (SWS_CORE_10099)
        /// @param text Explanatory text to be logged before aborting
        /// @note This function does not return. It terminates the process.
        [[noreturn]] void Abort(const char *text) noexcept;
    }
}

#endif
