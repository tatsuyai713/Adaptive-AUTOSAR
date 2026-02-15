/// @file src/ara/com/someip/rpc/socket_rpc_client.cpp
/// @brief Implementation for socket rpc client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./socket_rpc_client.h"
#include <functional>
#include <stdexcept>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                const vsomeip::instance_t SocketRpcClient::cInstanceId;

                SocketRpcClient::SocketRpcClient(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string ipAddress,
                    uint16_t port,
                    uint8_t protocolVersion,
                    uint8_t interfaceVersion) : RpcClient(protocolVersion, interfaceVersion),
                                                mApplication{VsomeipApplication::GetClientApplication()}
                {
                    (void)poller;
                    (void)ipAddress;
                    (void)port;

                    if (!mApplication)
                    {
                        throw std::runtime_error(
                            "vsomeip client application could not be created.");
                    }

                    mApplication->register_message_handler(
                        vsomeip::ANY_SERVICE,
                        vsomeip::ANY_INSTANCE,
                        vsomeip::ANY_METHOD,
                        std::bind(
                            &SocketRpcClient::onResponse,
                            this,
                            std::placeholders::_1));
                }

                SomeIpReturnCode SocketRpcClient::ConvertReturnCode(
                    vsomeip::return_code_e returnCode)
                {
                    switch (returnCode)
                    {
                    case vsomeip::return_code_e::E_OK:
                        return SomeIpReturnCode::eOK;
                    case vsomeip::return_code_e::E_NOT_OK:
                        return SomeIpReturnCode::eNotOk;
                    case vsomeip::return_code_e::E_UNKNOWN_SERVICE:
                        return SomeIpReturnCode::eUnknownService;
                    case vsomeip::return_code_e::E_UNKNOWN_METHOD:
                        return SomeIpReturnCode::eUnknownMethod;
                    case vsomeip::return_code_e::E_NOT_READY:
                        return SomeIpReturnCode::eNotReady;
                    case vsomeip::return_code_e::E_NOT_REACHABLE:
                        return SomeIpReturnCode::eNotReachable;
                    case vsomeip::return_code_e::E_TIMEOUT:
                        return SomeIpReturnCode::eTimeout;
                    case vsomeip::return_code_e::E_WRONG_PROTOCOL_VERSION:
                        return SomeIpReturnCode::eWrongProtocolVersion;
                    case vsomeip::return_code_e::E_WRONG_INTERFACE_VERSION:
                        return SomeIpReturnCode::eWrongInterfaceVersion;
                    case vsomeip::return_code_e::E_MALFORMED_MESSAGE:
                        return SomeIpReturnCode::eMalformedMessage;
                    case vsomeip::return_code_e::E_WRONG_MESSAGE_TYPE:
                        return SomeIpReturnCode::eWrongMessageType;
                    default:
                        return SomeIpReturnCode::eNotOk;
                    }
                }

                std::vector<uint8_t> SocketRpcClient::ConvertPayload(
                    const std::shared_ptr<vsomeip::payload> &payload)
                {
                    if (!payload)
                    {
                        return {};
                    }

                    const vsomeip::byte_t *cData{payload->get_data()};
                    const auto cLength{
                        static_cast<std::size_t>(payload->get_length())};

                    return std::vector<uint8_t>(cData, cData + cLength);
                }

                void SocketRpcClient::onResponse(
                    const std::shared_ptr<vsomeip::message> &message)
                {
                    if (!message)
                    {
                        return;
                    }

                    const uint32_t cMessageId{
                        (static_cast<uint32_t>(message->get_service()) << 16) |
                        static_cast<uint32_t>(message->get_method())};

                    const auto cReturnCode{
                        ConvertReturnCode(message->get_return_code())};
                    const auto cRpcPayload{
                        ConvertPayload(message->get_payload())};

                    SomeIpRpcMessage _response(
                        cMessageId,
                        message->get_client(),
                        message->get_session(),
                        message->get_protocol_version(),
                        message->get_interface_version(),
                        cReturnCode,
                        cRpcPayload);

                    InvokeHandler(_response.Payload());
                }

                void SocketRpcClient::Send(const std::vector<uint8_t> &payload)
                {
                    const SomeIpRpcMessage cRequest{
                        SomeIpRpcMessage::Deserialize(payload)};

                    const auto cService{
                        static_cast<vsomeip::service_t>(cRequest.MessageId() >> 16)};
                    const auto cMethod{
                        static_cast<vsomeip::method_t>(cRequest.MessageId() & 0xffff)};

                    {
                        std::lock_guard<std::mutex> _requestLock(mRequestMutex);
                        auto _insertResult{mRequestedServices.insert(cService)};
                        if (_insertResult.second)
                        {
                            mApplication->request_service(cService, cInstanceId);
                        }
                    }

                    auto _request{vsomeip::runtime::get()->create_request()};
                    _request->set_service(cService);
                    _request->set_instance(cInstanceId);
                    _request->set_method(cMethod);
                    _request->set_client(cRequest.ClientId());
                    _request->set_session(cRequest.SessionId());
                    _request->set_interface_version(cRequest.InterfaceVersion());

                    auto _rpcPayload{vsomeip::runtime::get()->create_payload()};
                    const auto &cRequestPayload{cRequest.RpcPayload()};
                    const std::vector<vsomeip::byte_t> cPayloadBytes(
                        cRequestPayload.begin(),
                        cRequestPayload.end());
                    _rpcPayload->set_data(cPayloadBytes);
                    _request->set_payload(_rpcPayload);

                    mApplication->send(_request);
                }

                SocketRpcClient::~SocketRpcClient()
                {
                    if (mApplication)
                    {
                        mApplication->unregister_message_handler(
                            vsomeip::ANY_SERVICE,
                            vsomeip::ANY_INSTANCE,
                            vsomeip::ANY_METHOD);

                        for (auto _service : mRequestedServices)
                        {
                            mApplication->release_service(_service, cInstanceId);
                        }
                    }
                }
            }
        }
    }
}
