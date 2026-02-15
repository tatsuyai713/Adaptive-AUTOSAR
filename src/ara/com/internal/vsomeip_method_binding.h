/// @file src/ara/com/internal/vsomeip_method_binding.h
/// @brief Declarations for vsomeip method binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_VSOMEIP_METHOD_BINDING_H
#define ARA_COM_INTERNAL_VSOMEIP_METHOD_BINDING_H

#include <cstdint>
#include <mutex>
#include "./method_binding.h"
#include "../com_error_domain.h"
#include "../someip/rpc/rpc_client.h"
#include "../someip/rpc/rpc_server.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief Configuration for a method binding
            struct MethodBindingConfig
            {
                std::uint16_t ServiceId;
                std::uint16_t InstanceId;
                std::uint16_t MethodId;
            };

            /// @brief vsomeip-based proxy-side method binding.
            ///        Wraps the existing RPC client to send requests and receive responses.
            class VsomeipProxyMethodBinding final : public ProxyMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                someip::rpc::RpcClient *mRpcClient;

            public:
                VsomeipProxyMethodBinding(
                    MethodBindingConfig config,
                    someip::rpc::RpcClient *rpcClient) noexcept;

                ~VsomeipProxyMethodBinding() noexcept override = default;

                void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) override;
            };

            /// @brief vsomeip-based skeleton-side method binding.
            ///        Wraps the existing RPC server to receive requests and send responses.
            class VsomeipSkeletonMethodBinding final : public SkeletonMethodBinding
            {
            private:
                MethodBindingConfig mConfig;
                someip::rpc::RpcServer *mRpcServer;

            public:
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

#endif
