/// @file src/ara/com/someip/rpc/socket_rpc_server.cpp
/// @brief Implementation for socket rpc server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./socket_rpc_server.h"
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
                const vsomeip::instance_t SocketRpcServer::cInstanceId;

                SocketRpcServer::SocketRpcServer(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string ipAddress,
                    uint16_t port,
                    uint8_t protocolVersion,
                    uint8_t interfaceVersion) : RpcServer(protocolVersion, interfaceVersion),
                                                mApplication{VsomeipApplication::GetServerApplication()}
                {
                    (void)poller;
                    (void)ipAddress;
                    (void)port;

                    if (!mApplication)
                    {
                        throw std::runtime_error(
                            "vsomeip server application could not be created.");
                    }
                }

                vsomeip::return_code_e SocketRpcServer::ConvertReturnCode(
                    SomeIpReturnCode returnCode)
                {
                    switch (returnCode)
                    {
                    case SomeIpReturnCode::eOK:
                        return vsomeip::return_code_e::E_OK;
                    case SomeIpReturnCode::eNotOk:
                        return vsomeip::return_code_e::E_NOT_OK;
                    case SomeIpReturnCode::eUnknownService:
                        return vsomeip::return_code_e::E_UNKNOWN_SERVICE;
                    case SomeIpReturnCode::eUnknownMethod:
                        return vsomeip::return_code_e::E_UNKNOWN_METHOD;
                    case SomeIpReturnCode::eNotReady:
                        return vsomeip::return_code_e::E_NOT_READY;
                    case SomeIpReturnCode::eNotReachable:
                        return vsomeip::return_code_e::E_NOT_REACHABLE;
                    case SomeIpReturnCode::eTimeout:
                        return vsomeip::return_code_e::E_TIMEOUT;
                    case SomeIpReturnCode::eWrongProtocolVersion:
                        return vsomeip::return_code_e::E_WRONG_PROTOCOL_VERSION;
                    case SomeIpReturnCode::eWrongInterfaceVersion:
                        return vsomeip::return_code_e::E_WRONG_INTERFACE_VERSION;
                    case SomeIpReturnCode::eMalformedMessage:
                        return vsomeip::return_code_e::E_MALFORMED_MESSAGE;
                    case SomeIpReturnCode::eWrongMessageType:
                        return vsomeip::return_code_e::E_WRONG_MESSAGE_TYPE;
                    default:
                        return vsomeip::return_code_e::E_NOT_OK;
                    }
                }

                std::vector<uint8_t> SocketRpcServer::ConvertPayload(
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

                void SocketRpcServer::OnHandlerRegistered(
                    uint16_t serviceId, uint16_t methodId)
                {
                    const auto cService{
                        static_cast<vsomeip::service_t>(serviceId)};
                    const auto cMethod{
                        static_cast<vsomeip::method_t>(methodId)};

                    {
                        std::lock_guard<std::mutex> _serviceLock(mServiceMutex);
                        auto _insertResult{mOfferedServices.insert(cService)};
                        if (_insertResult.second)
                        {
                            mApplication->offer_service(cService, cInstanceId);
                        }
                    }

                    mApplication->register_message_handler(
                        cService,
                        cInstanceId,
                        cMethod,
                        std::bind(
                            &SocketRpcServer::onRequest,
                            this,
                            std::placeholders::_1));
                }

                void SocketRpcServer::onRequest(
                    const std::shared_ptr<vsomeip::message> &requestMessage)
                {
                    if (!requestMessage)
                    {
                        return;
                    }

                    const uint32_t cMessageId{
                        (static_cast<uint32_t>(requestMessage->get_service()) << 16) |
                        static_cast<uint32_t>(requestMessage->get_method())};

                    const auto cRequestPayload{
                        ConvertPayload(requestMessage->get_payload())};

                    SomeIpRpcMessage _request(
                        cMessageId,
                        requestMessage->get_client(),
                        requestMessage->get_session(),
                        requestMessage->get_protocol_version(),
                        requestMessage->get_interface_version(),
                        cRequestPayload);

                    std::vector<uint8_t> _serializedResponse;
                    bool _handled{
                        TryInvokeHandler(_request.Payload(), _serializedResponse)};
                    if (!_handled)
                    {
                        return;
                    }

                    SomeIpRpcMessage _response{
                        SomeIpRpcMessage::Deserialize(_serializedResponse)};

                    auto _vsomeipResponse{
                        vsomeip::runtime::get()->create_response(requestMessage)};
                    _vsomeipResponse->set_return_code(
                        ConvertReturnCode(_response.ReturnCode()));
                    _vsomeipResponse->set_interface_version(
                        _response.InterfaceVersion());

                    auto _vsomeipPayload{vsomeip::runtime::get()->create_payload()};
                    const auto &cRpcPayload{_response.RpcPayload()};
                    const std::vector<vsomeip::byte_t> cPayloadBytes(
                        cRpcPayload.begin(),
                        cRpcPayload.end());
                    _vsomeipPayload->set_data(cPayloadBytes);
                    _vsomeipResponse->set_payload(_vsomeipPayload);

                    mApplication->send(_vsomeipResponse);
                }

                SocketRpcServer::~SocketRpcServer()
                {
                    if (mApplication)
                    {
                        for (auto _service : mOfferedServices)
                        {
                            mApplication->unregister_message_handler(
                                _service, cInstanceId, vsomeip::ANY_METHOD);
                            mApplication->stop_offer_service(
                                _service, cInstanceId);
                        }
                    }
                }
            }
        }
    }
}
