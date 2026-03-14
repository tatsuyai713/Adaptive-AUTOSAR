/// @file user_apps/src/apps/samples/03_communication_group.cpp
/// @brief Sample: CommunicationGroupServer and CommunicationGroupClient.
///
/// Demonstrates:
///   - CommunicationGroupServer<T,R>  — the "server" that sends T messages to
///                                      clients and receives R responses
///   - CommunicationGroupClient<T,R>  — the "client" that receives T messages
///                                      and sends R responses back
///   - Server API: AddClient, RemoveClient, Broadcast, Message, ListClients
///   - Client API: Message (receive), Response (send), SetResponseSender
///   - Fully in-process — no real network transport required
///
/// Scenario: a temperature controller (server) broadcasts alerts to sensor
/// clients; clients acknowledge with their measured value.
///
/// Expected output:
///   [cg] server broadcast 'HIGH_TEMP': sensor_1 received, sensor_2 received
///   [cg] server unicast to sensor_1 'CHECK_SENSOR': sensor_1 received
///   [cg] sensor_2 NOT notified by unicast (correct)
///   [cg] sensor_1 responds with 95
///   [cg] sensor_2 responds with 73
///   [cg] server received from 1: 95
///   [cg] server received from 2: 73
///   [cg] clients after removing sensor_2: [1]
///   [cg] all assertions passed

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "ara/com/cg/communication_group_client.h"
#include "ara/com/cg/communication_group_server.h"

int main()
{
    using namespace ara::com::cg;

    // T = std::string  (commands sent from server to clients)
    // R = std::uint32_t (temperature readings sent from clients to server)

    // ── Server setup ──────────────────────────────────────────────────────────

    std::vector<std::pair<std::uint32_t, std::uint32_t>> serverLog;

    CommunicationGroupServer<std::string, std::uint32_t> server{
        [&](std::uint32_t clientId, std::uint32_t reading)
        {
            serverLog.push_back({clientId, reading});
            std::cout << "[cg] server received from " << clientId
                      << ": " << reading << "\n";
        }};

    // ── Client setup ──────────────────────────────────────────────────────────

    std::vector<std::string> sensor1Log, sensor2Log;

    // Client 1: sensor_1 (ID = 1)
    CommunicationGroupClient<std::string, std::uint32_t> sensor1{
        [&](std::string cmd)
        {
            sensor1Log.push_back(cmd);
            std::cout << "[cg] server broadcast '" << cmd
                      << "': sensor_1 received\n";
        },
        [&](const std::uint32_t &reading)
        {
            server.Response(1U, reading); // route response back to server
        }};

    // Client 2: sensor_2 (ID = 2)
    CommunicationGroupClient<std::string, std::uint32_t> sensor2{
        [&](std::string cmd)
        {
            sensor2Log.push_back(cmd);
            std::cout << "[cg] server broadcast '" << cmd
                      << "': sensor_2 received\n";
        },
        [&](const std::uint32_t &reading)
        {
            server.Response(2U, reading);
        }};

    // ── Register clients with the server ─────────────────────────────────────
    //   Each AddClient call provides:
    //     - the client's numeric ID
    //     - a lambda that delivers a T message to that client

    server.AddClient(1U, [&](const std::string &cmd)
    {
        sensor1.Message(cmd); // in-process: directly call client's Message()
    });

    server.AddClient(2U, [&](const std::string &cmd)
    {
        sensor2.Message(cmd);
    });

    // ── Broadcast: send the same command to all clients ───────────────────────

    server.Broadcast("HIGH_TEMP").get();

    assert(sensor1Log.size() == 1U && sensor1Log[0] == "HIGH_TEMP");
    assert(sensor2Log.size() == 1U && sensor2Log[0] == "HIGH_TEMP");

    // ── Unicast: send a command to sensor_1 only ─────────────────────────────

    server.Message(1U, "CHECK_SENSOR").get();

    assert(sensor1Log.size() == 2U && sensor1Log[1] == "CHECK_SENSOR");
    assert(sensor2Log.size() == 1U); // sensor_2 not notified
    std::cout << "[cg] server unicast to sensor_1 'CHECK_SENSOR': sensor_1 received\n";
    std::cout << "[cg] sensor_2 NOT notified by unicast (correct)\n";

    // ── Clients respond ───────────────────────────────────────────────────────

    sensor1.Response(95U).get();  // sensor_1 reports 95°C
    sensor2.Response(73U).get();  // sensor_2 reports 73°C

    std::cout << "[cg] sensor_1 responds with 95\n";
    std::cout << "[cg] sensor_2 responds with 73\n";

    assert(serverLog.size() == 2U);
    assert(serverLog[0].first == 1U && serverLog[0].second == 95U);
    assert(serverLog[1].first == 2U && serverLog[1].second == 73U);

    // ── ListClients ───────────────────────────────────────────────────────────

    auto clients = server.ListClients().get();
    assert(clients.size() == 2U);

    // ── RemoveClient ──────────────────────────────────────────────────────────

    server.RemoveClient(2U);
    auto remaining = server.ListClients().get();
    assert(remaining.size() == 1U && remaining[0] == 1U);

    // Broadcast after removal — sensor_2 must not receive anything
    const std::size_t prevSensor2Size = sensor2Log.size();
    server.Broadcast("SHUTDOWN").get();
    assert(sensor2Log.size() == prevSensor2Size); // unchanged

    std::cout << "[cg] clients after removing sensor_2: [1]\n";
    std::cout << "[cg] all assertions passed\n";
    return 0;
}
