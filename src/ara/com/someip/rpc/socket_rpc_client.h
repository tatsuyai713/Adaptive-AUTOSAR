#ifndef SOCKET_RPC_CLIENT_H
#define SOCKET_RPC_CLIENT_H

#include <mutex>
#include <set>
#include "../vsomeip_application.h"
#include "./rpc_client.h"

namespace AsyncBsdSocketLib
{
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
                /// @brief TCP socket-based RPC client
                class SocketRpcClient : public RpcClient
                {
                private:
                    static const vsomeip::instance_t cInstanceId{1};
                    std::shared_ptr<vsomeip::application> mApplication;
                    std::set<vsomeip::service_t> mRequestedServices;
                    std::mutex mRequestMutex;

                    static SomeIpReturnCode ConvertReturnCode(
                        vsomeip::return_code_e returnCode);

                    static std::vector<uint8_t> ConvertPayload(
                        const std::shared_ptr<vsomeip::payload> &payload);

                    void onResponse(
                        const std::shared_ptr<vsomeip::message> &message);

                protected:
                    void Send(const std::vector<uint8_t> &payload) override;

                public:
                    /// @brief Constructor
                    /// @param poller BSD sockets poller
                    /// @param ipAddress RPC server IP address
                    /// @param port RPC server listening TCP port number
                    /// @param protocolVersion SOME/IP protocol header version
                    /// @param interfaceVersion Service interface version
                    /// @throws std::runtime_error Throws when the TCP client socket configuration failed
                    SocketRpcClient(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string ipAddress,
                        uint16_t port,
                        uint8_t protocolVersion,
                        uint8_t interfaceVersion = 1);

                    virtual ~SocketRpcClient() override;
                };
            }
        }
    }
}

#endif
