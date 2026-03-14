/// @file src/ara/com/cg/communication_group_client.h
/// @brief Declarations for communication group client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef COMMUNICATION_GROUP_CLIENT_H
#define COMMUNICATION_GROUP_CLIENT_H

#include <stdint.h>
#include <functional>
#include <future>

namespace ara
{
    namespace com
    {
        namespace cg
        {
            /// @brief Request message handler type
            /// @tparam R Request message type
            template <typename T>
            using RequestHandler = std::function<void(T)>;

            /// @brief Communication group client proxy.
            ///        The client receives typed request messages (T) from the server
            ///        and sends typed response messages (R) back.
            ///
            ///        Usage:
            ///        1. Construct with a request handler and an optional response sender.
            ///        2. Alternatively, call SetResponseSender() after construction.
            ///        3. Message() is invoked by the server or transport layer.
            ///        4. Call Response() to send a response back to the server.
            ///
            /// @tparam T Request message type (server -> client)
            /// @tparam R Response message type (client -> server)
            template <typename T, typename R>
            class CommunicationGroupClient
            {
            private:
                RequestHandler<T> mRequestHandler;
                std::function<void(const R &)> mResponseSender;

            public:
                /// @brief Constructor
                /// @param requestHandler  Callback invoked when the server sends a message.
                /// @param responseSender  Optional callable that delivers an R message to the server.
                explicit CommunicationGroupClient(
                    RequestHandler<T> requestHandler,
                    std::function<void(const R &)> responseSender = nullptr)
                    : mRequestHandler{requestHandler},
                      mResponseSender{std::move(responseSender)}
                {
                }

                ~CommunicationGroupClient() noexcept = default;

                CommunicationGroupClient(const CommunicationGroupClient &) = delete;
                CommunicationGroupClient &operator=(const CommunicationGroupClient &) = delete;

                /// @brief Set (or replace) the callable used to send response messages.
                /// @param sender Callable that delivers an R message to the server.
                void SetResponseSender(std::function<void(const R &)> sender)
                {
                    mResponseSender = std::move(sender);
                }

                /// @brief Receive a request message from the server.
                ///        Called by the transport layer or test harness.
                /// @param msg Received request message.
                void Message(const T &msg)
                {
                    mRequestHandler(msg);
                }

                /// @brief Send a response message to the server.
                /// @param responseMsg Response message to be sent.
                /// @returns Immediately-resolved fire-and-forget future.
                std::future<void> Response(const R &responseMsg) const
                {
                    if (mResponseSender)
                    {
                        mResponseSender(responseMsg);
                    }
                    std::promise<void> p;
                    p.set_value();
                    return p.get_future();
                }
            };
        }
    }
}

#endif
