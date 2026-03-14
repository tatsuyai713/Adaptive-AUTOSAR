/// @file test/ara/com/cg/communication_group_test.cpp
/// @brief Unit tests for CommunicationGroupServer and CommunicationGroupClient.

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "../../../../src/ara/com/cg/communication_group_client.h"
#include "../../../../src/ara/com/cg/communication_group_server.h"

namespace ara
{
    namespace com
    {
        namespace cg
        {
            // ── CommunicationGroupServer tests ──────────────────────────────

            TEST(CommunicationGroupServerTest, ListClientsEmptyInitially)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                auto clientsFuture = server.ListClients();
                auto clients = clientsFuture.get();
                EXPECT_TRUE(clients.empty());
            }

            TEST(CommunicationGroupServerTest, AddClientAppearsInListClients)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                server.AddClient(1U, [](const int &) {});
                server.AddClient(2U, [](const int &) {});

                auto clients = server.ListClients().get();
                ASSERT_EQ(clients.size(), 2U);
                EXPECT_NE(std::find(clients.begin(), clients.end(), 1U), clients.end());
                EXPECT_NE(std::find(clients.begin(), clients.end(), 2U), clients.end());
            }

            TEST(CommunicationGroupServerTest, RemoveClientRemovedFromList)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                server.AddClient(1U, [](const int &) {});
                server.AddClient(2U, [](const int &) {});
                server.RemoveClient(1U);

                auto clients = server.ListClients().get();
                ASSERT_EQ(clients.size(), 1U);
                EXPECT_EQ(clients.front(), 2U);
            }

            TEST(CommunicationGroupServerTest, BroadcastDeliversMsgToAllClients)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                std::vector<std::pair<uint32_t, int>> received;

                server.AddClient(10U, [&](const int &msg) {
                    received.push_back({10U, msg});
                });
                server.AddClient(20U, [&](const int &msg) {
                    received.push_back({20U, msg});
                });

                auto fut = server.Broadcast(42);
                fut.get(); // fire-and-forget resolves immediately

                ASSERT_EQ(received.size(), 2U);
                for (const auto &p : received)
                {
                    EXPECT_EQ(p.second, 42);
                }
            }

            TEST(CommunicationGroupServerTest, MessageDeliversMsgToSingleClient)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                std::vector<std::pair<uint32_t, int>> received;

                server.AddClient(10U, [&](const int &msg) {
                    received.push_back({10U, msg});
                });
                server.AddClient(20U, [&](const int &msg) {
                    received.push_back({20U, msg});
                });

                server.Message(10U, 99).get();

                ASSERT_EQ(received.size(), 1U);
                EXPECT_EQ(received[0].first, 10U);
                EXPECT_EQ(received[0].second, 99);
            }

            TEST(CommunicationGroupServerTest, MessageToUnknownClientIsNoOp)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                int callCount = 0;
                server.AddClient(1U, [&](const int &) { ++callCount; });

                server.Message(99U, 7).get(); // ID 99 not registered

                EXPECT_EQ(callCount, 0);
            }

            TEST(CommunicationGroupServerTest, ResponseDispatchesToHandler)
            {
                uint32_t receivedClientId = 0U;
                int receivedValue = 0;

                CommunicationGroupServer<int, int> server{
                    [&](uint32_t id, int val)
                    {
                        receivedClientId = id;
                        receivedValue = val;
                    }};

                server.Response(5U, 123);

                EXPECT_EQ(receivedClientId, 5U);
                EXPECT_EQ(receivedValue, 123);
            }

            TEST(CommunicationGroupServerTest, AddClientTwiceReplacesCallback)
            {
                CommunicationGroupServer<int, int> server{
                    [](uint32_t, int) {}};

                int counter1 = 0;
                int counter2 = 0;

                server.AddClient(1U, [&](const int &) { ++counter1; });
                server.AddClient(1U, [&](const int &) { ++counter2; }); // replace

                // Only one entry in list
                auto clients = server.ListClients().get();
                EXPECT_EQ(clients.size(), 1U);

                server.Broadcast(0).get();
                EXPECT_EQ(counter1, 0); // old callback replaced
                EXPECT_EQ(counter2, 1);
            }

            // ── CommunicationGroupClient tests ──────────────────────────────

            TEST(CommunicationGroupClientTest, MessageDispatchesToHandler)
            {
                int received = 0;

                CommunicationGroupClient<int, int> client{
                    [&](int msg) { received = msg; }};

                client.Message(77);

                EXPECT_EQ(received, 77);
            }

            TEST(CommunicationGroupClientTest, ResponseWithoutSenderSucceeds)
            {
                // No response sender set — should not crash
                CommunicationGroupClient<int, int> client{
                    [](int) {}};

                auto fut = client.Response(42);
                fut.get(); // should resolve immediately
            }

            TEST(CommunicationGroupClientTest, ResponseSendsViaConstructorSender)
            {
                int sentValue = 0;

                CommunicationGroupClient<int, int> client{
                    [](int) {},
                    [&](const int &r) { sentValue = r; }};

                client.Response(55).get();
                EXPECT_EQ(sentValue, 55);
            }

            TEST(CommunicationGroupClientTest, SetResponseSenderOverridesExisting)
            {
                int old = 0;
                int next = 0;

                CommunicationGroupClient<int, int> client{
                    [](int) {},
                    [&](const int &r) { old = r; }};

                client.SetResponseSender([&](const int &r) { next = r; });
                client.Response(7).get();

                EXPECT_EQ(old, 0);  // old sender not called
                EXPECT_EQ(next, 7); // new sender called
            }

            // ── Integration: server + client connected ───────────────────────

            TEST(CommunicationGroupIntegrationTest, ServerBroadcastClientResponds)
            {
                std::vector<int> clientReceived;
                int serverReceived = 0;
                uint32_t serverReceivedFrom = 0U;

                CommunicationGroupServer<int, int> server{
                    [&](uint32_t id, int val)
                    {
                        serverReceivedFrom = id;
                        serverReceived = val;
                    }};

                CommunicationGroupClient<int, int> client{
                    [&](int msg)
                    {
                        clientReceived.push_back(msg);
                        // Client responds with msg * 2
                    },
                    [&](const int &r)
                    {
                        server.Response(42U, r); // simulate transport to server
                    }};

                // Register client with server
                server.AddClient(42U, [&](const int &msg)
                {
                    client.Message(msg); // simulate transport to client
                });

                // Server broadcasts
                server.Broadcast(10).get();
                ASSERT_EQ(clientReceived.size(), 1U);
                EXPECT_EQ(clientReceived[0], 10);

                // Client responds
                client.Response(20).get();
                EXPECT_EQ(serverReceivedFrom, 42U);
                EXPECT_EQ(serverReceived, 20);
            }

        } // namespace cg
    }     // namespace com
}         // namespace ara
