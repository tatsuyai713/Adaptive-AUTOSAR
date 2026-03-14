/// @file src/ara/com/internal/vsomeip_method_binding.h
/// @brief Declarations for vsomeip method binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_VSOMEIP_METHOD_BINDING_H
#define ARA_COM_INTERNAL_VSOMEIP_METHOD_BINDING_H

#include <cstdint>
#include <memory>
#include <mutex>
#include "./method_binding.h"
#include "../com_error_domain.h"
#include "../someip/rpc/rpc_client.h"
#include "../someip/rpc/rpc_server.h"
#include "../someip/rpc/socket_rpc_client.h"
#include "../someip/rpc/socket_rpc_server.h"

#if ARA_COM_USE_VSOMEIP

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief vsomeip-based proxy-side method binding.
            ///        Self-contained: internally creates and owns a SocketRpcClient
            ///        when constructed with only a MethodBindingConfig (factory path).
            ///        Also supports external RpcClient injection for legacy callers.
            class VsomeipProxyMethodBinding final : public ProxyMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                someip::rpc::RpcClient *mRpcClient{nullptr};
                std::unique_ptr<someip::rpc::SocketRpcClient> mOwnedClient;

            public:
                /// @brief Self-contained constructor (for BindingFactory).
                ///        Creates its own SocketRpcClient from config.
                explicit VsomeipProxyMethodBinding(
                    MethodBindingConfig config);

                /// @brief Legacy constructor with externally-managed RpcClient.
                VsomeipProxyMethodBinding(
                    MethodBindingConfig config,
                    someip::rpc::RpcClient *rpcClient) noexcept;

                ~VsomeipProxyMethodBinding() noexcept override = default;

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override;
            };

            /// @brief vsomeip-based skeleton-side method binding.
            ///        Self-contained: internally creates and owns a SocketRpcServer
            ///        when constructed with only a MethodBindingConfig (factory path).
            ///        Also supports external RpcServer injection for legacy callers.
            class VsomeipSkeletonMethodBinding final : public SkeletonMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                someip::rpc::RpcServer *mRpcServer{nullptr};
                std::unique_ptr<someip::rpc::SocketRpcServer> mOwnedServer;

            public:
                /// @brief Self-contained constructor (for BindingFactory).
                ///        Creates its own SocketRpcServer from config.
                explicit VsomeipSkeletonMethodBinding(
                    MethodBindingConfig config);

                /// @brief Legacy constructor with externally-managed RpcServer.
                VsomeipSkeletonMethodBinding(
                    MethodBindingConfig config,
                    someip::rpc::RpcServer *rpcServer) noexcept;

                ~VsomeipSkeletonMethodBinding() noexcept override;

                core::Result<void> Register(
                    RawRequestHandler handler) override;
                void Unregister() override;
            };
        }
    }
}

#endif // ARA_COM_USE_VSOMEIP

#endif
