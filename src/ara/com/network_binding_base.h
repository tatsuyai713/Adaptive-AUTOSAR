/// @file src/ara/com/network_binding_base.h
/// @brief Abstract network binding interface per SWS_CM_00610.
/// @details Defines the base abstraction for transport-level network bindings.
///          Each concrete binding (SOME/IP, DDS, IPC) implements this interface
///          to provide connection lifecycle management and health monitoring.

#ifndef ARA_COM_NETWORK_BINDING_BASE_H
#define ARA_COM_NETWORK_BINDING_BASE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include "./com_error_domain.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Transport binding type discriminator.
        enum class BindingType : std::uint8_t
        {
            kSomeIp = 0U,       ///< SOME/IP over UDP/TCP.
            kDds = 1U,          ///< DDS (Data Distribution Service).
            kIpc = 2U,          ///< Local IPC (e.g., iceoryx, Unix domain socket).
            kUserDefined = 3U   ///< Custom user-defined binding.
        };

        /// @brief Connection health status.
        enum class ConnectionState : std::uint8_t
        {
            kDisconnected = 0U,    ///< Not connected.
            kConnecting = 1U,      ///< Connection in progress.
            kConnected = 2U,       ///< Fully connected and operational.
            kDegraded = 3U,        ///< Connected but experiencing issues.
            kReconnecting = 4U     ///< Lost connection, attempting to reconnect.
        };

        /// @brief Abstract base class for network bindings per SWS_CM_00610.
        ///        Transport implementations (SOME/IP, DDS, IPC) derive from this
        ///        and provide concrete connection management.
        class NetworkBindingBase
        {
        public:
            /// @brief Callback for connection state changes.
            using ConnectionStateHandler =
                std::function<void(ConnectionState)>;

            virtual ~NetworkBindingBase() noexcept = default;

            /// @brief Get the binding type.
            /// @returns The transport binding discriminator.
            virtual BindingType GetBindingType() const noexcept = 0;

            /// @brief Get a human-readable binding name.
            /// @returns Binding name string (e.g., "SOME/IP", "CycloneDDS").
            virtual const char *GetBindingName() const noexcept = 0;

            /// @brief Initialize the binding.
            /// @returns Result indicating success or initialization failure.
            virtual core::Result<void> Initialize() = 0;

            /// @brief Shutdown the binding and release resources.
            virtual void Shutdown() = 0;

            /// @brief Get current connection state.
            /// @returns Current state of the binding connection.
            virtual ConnectionState GetConnectionState() const noexcept = 0;

            /// @brief Set handler for connection state changes.
            /// @param handler Callback invoked on state transitions.
            virtual void SetConnectionStateHandler(
                ConnectionStateHandler handler) = 0;

            /// @brief Check if the binding is healthy and can send/receive.
            /// @returns True if the binding is in a connected state.
            virtual bool IsHealthy() const noexcept
            {
                auto state = GetConnectionState();
                return state == ConnectionState::kConnected;
            }

            /// @brief Get binding-specific statistics as key-value description.
            /// @returns Human-readable statistics string.
            virtual std::string GetStatistics() const
            {
                return "No statistics available";
            }
        };

        /// @brief Local IPC binding base per SWS_CM_00611.
        ///        Provides in-process and local inter-process communication.
        class LocalIpcBinding : public NetworkBindingBase
        {
        private:
            ConnectionState mState{ConnectionState::kDisconnected};
            ConnectionStateHandler mHandler;

        public:
            BindingType GetBindingType() const noexcept override
            {
                return BindingType::kIpc;
            }

            const char *GetBindingName() const noexcept override
            {
                return "LocalIPC";
            }

            core::Result<void> Initialize() override
            {
                mState = ConnectionState::kConnected;
                NotifyState();
                return core::Result<void>::FromValue();
            }

            void Shutdown() override
            {
                mState = ConnectionState::kDisconnected;
                NotifyState();
            }

            ConnectionState GetConnectionState() const noexcept override
            {
                return mState;
            }

            void SetConnectionStateHandler(
                ConnectionStateHandler handler) override
            {
                mHandler = std::move(handler);
            }

        private:
            void NotifyState()
            {
                if (mHandler)
                {
                    mHandler(mState);
                }
            }
        };
    }
}

#endif
