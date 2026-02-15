/// @file src/ara/com/someip/rpc/socket_rpc_server.h
/// @brief Declarations for socket rpc server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef SOCKET_RPC_SERVER_H
#define SOCKET_RPC_SERVER_H

#include <memory>
#include <mutex>
#include <set>
#include "../vsomeip_application.h"
#include "./rpc_server.h"

namespace AsyncBsdSocketLib
{
    /// @brief Forward declaration of async socket poller.
    class Poller;
}

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                /// @brief TCP socket-based RPC server
                class SocketRpcServer : public RpcServer
                {
                private:
                    static const vsomeip::instance_t cInstanceId{1};
                    std::shared_ptr<vsomeip::application> mApplication;
                    std::set<vsomeip::service_t> mOfferedServices;
                    std::mutex mServiceMutex;

                    static vsomeip::return_code_e ConvertReturnCode(
                        SomeIpReturnCode returnCode);

                    static std::vector<uint8_t> ConvertPayload(
                        const std::shared_ptr<vsomeip::payload> &payload);

                    void onRequest(
                        const std::shared_ptr<vsomeip::message> &requestMessage);

                protected:
                    void OnHandlerRegistered(
                        uint16_t serviceId, uint16_t methodId) override;

                public:
                    /// @brief Constructor
                    /// @param poller BSD sockets poller
                    /// @param ipAddress RPC server IP address
                    /// @param port RPC server listening TCP port number
                    /// @param protocolVersion SOME/IP protocol header version
                    /// @param interfaceVersion Service interface version
                    /// @throws std::runtime_error Throws when the TCP server socket configuration failed
                    SocketRpcServer(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string ipAddress,
                        uint16_t port,
                        uint8_t protocolVersion,
                        uint8_t interfaceVersion = 1);

                    virtual ~SocketRpcServer() override;
                };
            }
        }
    }
}

#endif
