#include "./vsomeip_method_binding.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            // ── VsomeipProxyMethodBinding ──────────────────────────

            VsomeipProxyMethodBinding::VsomeipProxyMethodBinding(
                MethodBindingConfig config,
                someip::rpc::RpcClient *rpcClient) noexcept
                : mConfig{config},
                  mRpcClient{rpcClient}
            {
            }

            void VsomeipProxyMethodBinding::Call(
                const std::vector<std::uint8_t> &requestPayload,
                RawResponseHandler responseHandler)
            {
                if (!mRpcClient)
                {
                    responseHandler(
                        core::Result<std::vector<std::uint8_t>>::FromError(
                            MakeErrorCode(ComErrc::kNetworkBindingFailure)));
                    return;
                }

                // Set up the response handler on the RPC client
                mRpcClient->SetHandler(
                    mConfig.ServiceId,
                    mConfig.MethodId,
                    [responseHandler](
                        const someip::rpc::SomeIpRpcMessage &response)
                    {
                        responseHandler(
                            core::Result<std::vector<std::uint8_t>>::FromValue(
                                response.RpcPayload()));
                    });

                // Use the public Send overload: Send(serviceId, methodId, clientId, rpcPayload)
                // clientId 0 is used as a default; the RPC client manages session IDs internally
                mRpcClient->Send(
                    mConfig.ServiceId,
                    mConfig.MethodId,
                    0U,
                    requestPayload);
            }

            // ── VsomeipSkeletonMethodBinding ──────────────────────

            VsomeipSkeletonMethodBinding::VsomeipSkeletonMethodBinding(
                MethodBindingConfig config,
                someip::rpc::RpcServer *rpcServer) noexcept
                : mConfig{config},
                  mRpcServer{rpcServer}
            {
            }

            VsomeipSkeletonMethodBinding::~VsomeipSkeletonMethodBinding() noexcept
            {
                Unregister();
            }

            core::Result<void> VsomeipSkeletonMethodBinding::Register(
                RawRequestHandler handler)
            {
                if (!mRpcServer)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kNetworkBindingFailure));
                }

                if (!handler)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kSetHandlerNotSet));
                }

                // RpcServer::HandlerType = function<bool(const vector<uint8_t>&, vector<uint8_t>&)>
                mRpcServer->SetHandler(
                    mConfig.ServiceId,
                    mConfig.MethodId,
                    [handler](
                        const std::vector<std::uint8_t> &requestPayload,
                        std::vector<std::uint8_t> &responsePayload) -> bool
                    {
                        auto result = handler(requestPayload);
                        if (result.HasValue())
                        {
                            responsePayload = result.Value();
                            return true;
                        }
                        return false;
                    });

                return core::Result<void>::FromValue();
            }

            void VsomeipSkeletonMethodBinding::Unregister()
            {
                if (mRpcServer)
                {
                    mRpcServer->SetHandler(
                        mConfig.ServiceId,
                        mConfig.MethodId,
                        nullptr);
                }
            }
        }
    }
}
