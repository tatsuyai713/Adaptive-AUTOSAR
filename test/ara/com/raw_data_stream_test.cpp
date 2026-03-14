#include <gtest/gtest.h>
#include "../../../src/ara/com/raw_data_stream.h"

namespace ara
{
    namespace com
    {
        // ── RawDataStreamServer Tests ──────────────────────

        TEST(RawDataStreamServerTest, InitialState)
        {
            RawDataStreamServer server;
            EXPECT_EQ(server.GetState(), StreamState::kClosed);
            EXPECT_EQ(server.GetTotalBytesReceived(), 0U);
            EXPECT_EQ(server.GetBufferedSize(), 0U);
        }

        TEST(RawDataStreamServerTest, OpenAndClose)
        {
            RawDataStreamServer server;
            auto result = server.Open();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(server.GetState(), StreamState::kOpen);

            server.Close();
            EXPECT_EQ(server.GetState(), StreamState::kClosed);
        }

        TEST(RawDataStreamServerTest, ReceiveData)
        {
            RawDataStreamServer server;
            server.Open();

            std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04};
            auto result = server.OnDataReceived(data.data(), data.size());
            ASSERT_TRUE(result.HasValue());

            EXPECT_EQ(server.GetTotalBytesReceived(), 4U);
            EXPECT_EQ(server.GetBufferedSize(), 4U);
        }

        TEST(RawDataStreamServerTest, ReadBufferedData)
        {
            RawDataStreamServer server;
            server.Open();

            std::vector<std::uint8_t> data = {0x0A, 0x0B, 0x0C};
            server.OnDataReceived(data.data(), data.size());

            auto readResult = server.Read();
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(readResult.Value().size(), 3U);
            EXPECT_EQ(readResult.Value()[0], 0x0A);
            EXPECT_EQ(readResult.Value()[1], 0x0B);
            EXPECT_EQ(readResult.Value()[2], 0x0C);

            EXPECT_EQ(server.GetBufferedSize(), 0U);
        }

        TEST(RawDataStreamServerTest, ReadWithLimit)
        {
            RawDataStreamServer server;
            server.Open();

            std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
            server.OnDataReceived(data.data(), data.size());

            auto readResult = server.Read(3);
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(readResult.Value().size(), 3U);
            EXPECT_EQ(server.GetBufferedSize(), 2U);
        }

        TEST(RawDataStreamServerTest, ReceiveWhenClosedFails)
        {
            RawDataStreamServer server;
            std::vector<std::uint8_t> data = {0x01};
            auto result = server.OnDataReceived(data.data(), data.size());
            EXPECT_FALSE(result.HasValue());
        }

        TEST(RawDataStreamServerTest, DataHandlerCalled)
        {
            RawDataStreamServer server;
            server.Open();

            std::size_t handlerCalls = 0;
            server.SetDataHandler(
                [&handlerCalls](const std::uint8_t *, std::size_t)
                {
                    ++handlerCalls;
                });

            std::vector<std::uint8_t> data = {0x01, 0x02};
            server.OnDataReceived(data.data(), data.size());
            EXPECT_EQ(handlerCalls, 1U);
        }

        TEST(RawDataStreamServerTest, BufferOverflowSuspends)
        {
            RawDataStreamServer server(10); // 10 byte max buffer
            server.Open();

            std::vector<std::uint8_t> data(8, 0xFF);
            auto result1 = server.OnDataReceived(data.data(), data.size());
            ASSERT_TRUE(result1.HasValue());

            std::vector<std::uint8_t> overflow(5, 0xAA); // exceeds 10
            auto result2 = server.OnDataReceived(overflow.data(), overflow.size());
            EXPECT_FALSE(result2.HasValue());
            EXPECT_EQ(server.GetState(), StreamState::kSuspended);
        }

        TEST(RawDataStreamServerTest, ReadResumesFromSuspended)
        {
            RawDataStreamServer server(10);
            server.Open();

            std::vector<std::uint8_t> data(8, 0xFF);
            server.OnDataReceived(data.data(), data.size());

            // Suspend
            std::vector<std::uint8_t> overflow(5, 0xAA);
            server.OnDataReceived(overflow.data(), overflow.size());
            EXPECT_EQ(server.GetState(), StreamState::kSuspended);

            // Read all -> should resume
            server.Read();
            EXPECT_EQ(server.GetState(), StreamState::kOpen);
        }

        TEST(RawDataStreamServerTest, StateHandlerCalled)
        {
            RawDataStreamServer server;
            std::vector<StreamState> states;
            server.SetStateHandler(
                [&states](StreamState s)
                {
                    states.push_back(s);
                });

            server.Open();
            server.Close();

            ASSERT_EQ(states.size(), 2U);
            EXPECT_EQ(states[0], StreamState::kOpen);
            EXPECT_EQ(states[1], StreamState::kClosed);
        }

        TEST(RawDataStreamServerTest, MultipleChunksAccumulate)
        {
            RawDataStreamServer server;
            server.Open();

            std::vector<std::uint8_t> c1 = {0x01, 0x02};
            std::vector<std::uint8_t> c2 = {0x03, 0x04};
            server.OnDataReceived(c1.data(), c1.size());
            server.OnDataReceived(c2.data(), c2.size());

            EXPECT_EQ(server.GetBufferedSize(), 4U);
            EXPECT_EQ(server.GetTotalBytesReceived(), 4U);

            auto readResult = server.Read();
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(readResult.Value().size(), 4U);
        }

        // ── RawDataStreamClient Tests ──────────────────────

        TEST(RawDataStreamClientTest, InitialState)
        {
            RawDataStreamClient client;
            EXPECT_EQ(client.GetState(), StreamState::kClosed);
            EXPECT_EQ(client.GetTotalBytesSent(), 0U);
        }

        TEST(RawDataStreamClientTest, ConnectAndDisconnect)
        {
            RawDataStreamServer server;
            server.Open();

            RawDataStreamClient client;
            auto result = client.Connect(server);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(client.GetState(), StreamState::kOpen);

            client.Disconnect();
            EXPECT_EQ(client.GetState(), StreamState::kClosed);
        }

        TEST(RawDataStreamClientTest, WriteData)
        {
            RawDataStreamServer server;
            server.Open();

            RawDataStreamClient client;
            client.Connect(server);

            std::vector<std::uint8_t> data = {0x10, 0x20, 0x30};
            auto result = client.Write(data);
            ASSERT_TRUE(result.HasValue());

            EXPECT_EQ(client.GetTotalBytesSent(), 3U);
            EXPECT_EQ(server.GetTotalBytesReceived(), 3U);
        }

        TEST(RawDataStreamClientTest, WriteWhenDisconnectedFails)
        {
            RawDataStreamClient client;
            std::vector<std::uint8_t> data = {0x01};
            auto result = client.Write(data);
            EXPECT_FALSE(result.HasValue());
        }

        TEST(RawDataStreamClientTest, WriteAndRead)
        {
            RawDataStreamServer server;
            server.Open();

            RawDataStreamClient client;
            client.Connect(server);

            std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD};
            client.Write(data);

            auto readResult = server.Read();
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(readResult.Value(), data);
        }

        TEST(RawDataStreamClientTest, StateHandlerCalled)
        {
            RawDataStreamServer server;
            server.Open();

            RawDataStreamClient client;
            std::vector<StreamState> states;
            client.SetStateHandler(
                [&states](StreamState s)
                {
                    states.push_back(s);
                });

            client.Connect(server);
            client.Disconnect();

            ASSERT_EQ(states.size(), 2U);
            EXPECT_EQ(states[0], StreamState::kOpen);
            EXPECT_EQ(states[1], StreamState::kClosed);
        }

        TEST(RawDataStreamClientTest, BackpressureSuspends)
        {
            RawDataStreamServer server(10);
            server.Open();

            RawDataStreamClient client;
            client.Connect(server);

            std::vector<std::uint8_t> data(8, 0xFF);
            client.Write(data);

            std::vector<std::uint8_t> overflow(5, 0xAA);
            auto result = client.Write(overflow);
            EXPECT_FALSE(result.HasValue());
            EXPECT_EQ(client.GetState(), StreamState::kSuspended);
        }
    }
}
