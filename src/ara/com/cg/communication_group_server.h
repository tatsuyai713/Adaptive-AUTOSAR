/// @file src/ara/com/cg/communication_group_server.h
/// @brief Declarations for communication group server.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef COMMUNICATION_GROUP_SERVER_H
#define COMMUNICATION_GROUP_SERVER_H

#include <stdint.h>
#include <algorithm>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <vector>

namespace ara
{
    namespace com
    {
        namespace cg
        {
            /// @brief Response message handler type
            /// @tparam R Response message type
            template <typename R>
            using ResponseHandler = std::function<void(uint32_t, R)>;

            /// @brief Communication group server skeleton.
            ///        The server sends typed request messages (T) to registered clients
            ///        and receives typed response messages (R) from them.
            ///
            ///        Usage:
            ///        1. Construct with a response handler.
            ///        2. Register clients via AddClient(id, senderFn).
            ///        3. Call Broadcast() / Message() to send to clients.
            ///        4. Response() is invoked automatically when a client responds.
            ///
            /// @tparam T Request message type (server -> client)
            /// @tparam R Response message type (client -> server)
            template <typename T, typename R>
            class CommunicationGroupServer
            {
            private:
                ResponseHandler<R> mResponseHandler;
                mutable std::mutex mMutex;
                std::vector<uint32_t> mClients;
                std::map<uint32_t, std::function<void(const T &)>> mClientSenders;

            public:
                /// @brief Constructor
                /// @param responseHandler Callback invoked when a client sends a response.
                explicit CommunicationGroupServer(ResponseHandler<R> responseHandler)
                    : mResponseHandler{responseHandler}
                {
                }

                ~CommunicationGroupServer() noexcept = default;

                CommunicationGroupServer(const CommunicationGroupServer &) = delete;
                CommunicationGroupServer &operator=(const CommunicationGroupServer &) = delete;

                /// @brief Register a client with a unicast sender callback.
                /// @param clientId Unique client identifier.
                /// @param sender   Callable that delivers a T message to this client.
                void AddClient(uint32_t clientId, std::function<void(const T &)> sender)
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mClientSenders.find(clientId) == mClientSenders.end())
                    {
                        mClients.push_back(clientId);
                    }
                    mClientSenders[clientId] = std::move(sender);
                }

                /// @brief Unregister a client.
                /// @param clientId Client to remove.
                void RemoveClient(uint32_t clientId)
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mClients.erase(
                        std::remove(mClients.begin(), mClients.end(), clientId),
                        mClients.end());
                    mClientSenders.erase(clientId);
                }

                /// @brief Broadcast a request message to all registered clients.
                /// @param msg Request message to be broadcasted.
                /// @returns Immediately-resolved fire-and-forget future.
                std::future<void> Broadcast(const T &msg) const
                {
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        for (const auto &kv : mClientSenders)
                        {
                            kv.second(msg);
                        }
                    }
                    std::promise<void> p;
                    p.set_value();
                    return p.get_future();
                }

                /// @brief Send a request message to a specific client.
                /// @param clientID Target client identifier.
                /// @param msg      Request message to be sent.
                /// @returns Immediately-resolved fire-and-forget future.
                std::future<void> Message(uint32_t clientID, const T &msg) const
                {
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        auto it = mClientSenders.find(clientID);
                        if (it != mClientSenders.end())
                        {
                            it->second(msg);
                        }
                    }
                    std::promise<void> p;
                    p.set_value();
                    return p.get_future();
                }

                /// @brief Receive a response message from a client.
                ///        Called by the transport layer or test harness.
                /// @param clientID     Responding client identifier.
                /// @param responseMsg  Received response message.
                void Response(uint32_t clientID, const R &responseMsg)
                {
                    mResponseHandler(clientID, responseMsg);
                }

                /// @brief List all currently registered clients.
                /// @returns Future resolved with the vector of client IDs.
                std::future<std::vector<uint32_t>> ListClients() const
                {
                    std::vector<uint32_t> clients;
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        clients = mClients;
                    }
                    std::promise<std::vector<uint32_t>> p;
                    p.set_value(std::move(clients));
                    return p.get_future();
                }
            };
        }
    }
}

#endif
