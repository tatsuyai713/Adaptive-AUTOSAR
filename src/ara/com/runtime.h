/// @file src/ara/com/runtime.h
/// @brief ara::com runtime lifecycle per AUTOSAR AP SWS_CM_00001/SWS_CM_00002.
/// @details Provides Initialize/Deinitialize for the Communication Management
///          runtime. All Proxy/Skeleton operations require a prior Initialize().

#ifndef ARA_COM_RUNTIME_H
#define ARA_COM_RUNTIME_H

#include <atomic>
#include <mutex>
#include <string>
#include "../core/result.h"
#include "../core/instance_specifier.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        /// @brief Communication Management runtime lifecycle per SWS_CM_00001.
        namespace runtime
        {
            namespace detail
            {
                /// @brief Internal state for the CM runtime.
                struct RuntimeState
                {
                    std::atomic<bool> Initialized{false};
                    std::string ApplicationName;
                    std::mutex Mutex;
                };

                inline RuntimeState &GetState() noexcept
                {
                    static RuntimeState state;
                    return state;
                }
            } // namespace detail

            /// @brief Initialize the ara::com runtime (SWS_CM_00001).
            /// @param specifier Application instance specifier from the manifest.
            /// @returns Success or error if already initialized.
            inline core::Result<void> Initialize(
                const core::InstanceSpecifier &specifier)
            {
                auto &state = detail::GetState();
                std::lock_guard<std::mutex> lock(state.Mutex);

                if (state.Initialized.load())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                state.ApplicationName = specifier.ToString();
                state.Initialized.store(true);
                return core::Result<void>::FromValue();
            }

            /// @brief Initialize the ara::com runtime with default configuration.
            /// @returns Success or error if already initialized.
            inline core::Result<void> Initialize()
            {
                return Initialize(
                    core::InstanceSpecifier{"default_application"});
            }

            /// @brief Deinitialize the ara::com runtime (SWS_CM_00002).
            /// @returns Success or error if not initialized.
            inline core::Result<void> Deinitialize()
            {
                auto &state = detail::GetState();
                std::lock_guard<std::mutex> lock(state.Mutex);

                if (!state.Initialized.load())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kCommunicationStackError));
                }

                state.ApplicationName.clear();
                state.Initialized.store(false);
                return core::Result<void>::FromValue();
            }

            /// @brief Check whether the ara::com runtime is initialized.
            /// @returns true if Initialize() has been called successfully.
            inline bool IsInitialized() noexcept
            {
                return detail::GetState().Initialized.load();
            }

            /// @brief Get the application name set during initialization.
            /// @returns Application name string (empty if not initialized).
            inline const std::string &GetApplicationName() noexcept
            {
                return detail::GetState().ApplicationName;
            }
        } // namespace runtime
    }
}

#endif
