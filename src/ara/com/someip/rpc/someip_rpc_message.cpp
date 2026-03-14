/// @file src/ara/com/someip/rpc/someip_rpc_message.cpp
/// @brief Implementation for someip rpc message.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <utility>
#include "./someip_rpc_message.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                SomeIpRpcMessage::SomeIpRpcMessage() : SomeIpMessage(0,
                                                                     0,
                                                                     1,
                                                                     1,
                                                                     SomeIpMessageType::Request)
                {
                }

                SomeIpRpcMessage::SomeIpRpcMessage(uint32_t messageId,
                                                   uint16_t clientId,
                                                   uint16_t sessionId,
                                                   uint8_t protocolVersion,
                                                   uint8_t interfaceVersion,
                                                   const std::vector<uint8_t> &rpcPayload) : SomeIpMessage(messageId,
                                                                                                           clientId,
                                                                                                           protocolVersion,
                                                                                                           interfaceVersion,
                                                                                                           SomeIpMessageType::Request,
                                                                                                           sessionId),
                                                                                             mRpcPayload{rpcPayload}
                {
                }

                SomeIpRpcMessage::SomeIpRpcMessage(uint32_t messageId,
                                                   uint16_t clientId,
                                                   uint16_t sessionId,
                                                   uint8_t protocolVersion,
                                                   uint8_t interfaceVersion,
                                                   std::vector<uint8_t> &&rpcPayload) : SomeIpMessage(messageId,
                                                                                                      clientId,
                                                                                                      protocolVersion,
                                                                                                      interfaceVersion,
                                                                                                      SomeIpMessageType::Request,
                                                                                                      sessionId),
                                                                                        mRpcPayload{std::move(rpcPayload)}
                {
                }

                SomeIpRpcMessage::SomeIpRpcMessage(uint32_t messageId,
                                                   uint16_t clientId,
                                                   uint16_t sessionId,
                                                   uint8_t protocolVersion,
                                                   uint8_t interfaceVersion,
                                                   SomeIpReturnCode returnCode,
                                                   const std::vector<uint8_t> &rpcPayload) : SomeIpMessage(messageId,
                                                                                                           clientId,
                                                                                                           protocolVersion,
                                                                                                           interfaceVersion,
                                                                                                           returnCode == SomeIpReturnCode::eOK ? SomeIpMessageType::Response : SomeIpMessageType::Error,
                                                                                                           returnCode,
                                                                                                           sessionId),
                                                                                             mRpcPayload{rpcPayload}
                {
                }

                uint32_t SomeIpRpcMessage::Length() const noexcept
                {
                    const size_t cHeaderLength{8};
                    const size_t cTpHeaderLength = IsTp() ? 4U : 0U;
                    auto _result{
                        static_cast<uint32_t>(
                            cHeaderLength + cTpHeaderLength + mRpcPayload.size())};

                    return _result;
                }

                std::vector<uint8_t> SomeIpRpcMessage::Payload() const
                {
                    // General SOME/IP header payload insertion
                    std::vector<uint8_t> _result = SomeIpMessage::Payload();
                    _result.insert(
                        _result.end(), mRpcPayload.cbegin(), mRpcPayload.cend());

                    return _result;
                }

                const std::vector<uint8_t> &SomeIpRpcMessage::RpcPayload() const
                {
                    return mRpcPayload;
                }

                SomeIpRpcMessage SomeIpRpcMessage::Deserialize(
                    const std::vector<uint8_t> &payload)
                {
                    const size_t cHeaderSize{16};
                    const size_t cTpHeaderSize{4};

                    size_t _lengthOffset{4};
                    uint32_t _lengthInt{
                        helper::ExtractInteger(payload, _lengthOffset)};
                    auto _length{static_cast<size_t>(_lengthInt)};

                    SomeIpRpcMessage _result;
                    SomeIpMessage::Deserialize(&_result, payload);

                    // For TP messages the 4-byte TP header sits at bytes 16-19
                    // and the RPC payload begins at byte 20.
                    const size_t payloadStart =
                        _result.IsTp()
                            ? cHeaderSize + cTpHeaderSize
                            : cHeaderSize;

                    const size_t payloadEnd =
                        cHeaderSize + _length - _lengthOffset;

                    if (payloadStart <= payloadEnd && payloadEnd <= payload.size())
                    {
                        _result.mRpcPayload =
                            std::vector<uint8_t>(
                                payload.cbegin() + payloadStart,
                                payload.cbegin() + payloadEnd);
                    }

                    return _result;
                }
            }
        }
    }
}