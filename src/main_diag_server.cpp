/// @file src/main_diag_server.cpp
/// @brief Resident daemon that runs the AUTOSAR Diagnostic Server (UDS/DoIP).
/// @details Provides a dedicated diagnostic service endpoint, listening for
///          UDS requests on a configurable DoIP TCP port and processing them
///          through the ara::diag DiagnosticManager. Writes periodic status
///          to /run/autosar/diag_server.status.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "./ara/core/initialization.h"
#include "./ara/diag/diagnostic_manager.h"

namespace
{
    std::atomic_bool gRunning{true};

    void RequestStop(int) noexcept
    {
        gRunning = false;
    }

    std::string GetEnvOrDefault(const char *key, std::string fallback)
    {
        const char *value{std::getenv(key)};
        if (value != nullptr)
        {
            return value;
        }

        return fallback;
    }

    std::uint32_t GetEnvU32(const char *key, std::uint32_t fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(value)};
            if (parsed > 600000U)
            {
                return fallback;
            }
            return static_cast<std::uint32_t>(parsed);
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::uint64_t NowEpochMs()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    /// Open a non-blocking TCP server socket for DoIP diagnostic requests.
    int OpenListenSocket(const std::string &listenAddr,
                         std::uint16_t listenPort)
    {
        int fd{::socket(AF_INET, SOCK_STREAM, 0)};
        if (fd < 0)
        {
            return -1;
        }

        int reuseAddr{1};
        (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                           &reuseAddr, sizeof(reuseAddr));

        struct sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(listenPort);
        ::inet_pton(AF_INET, listenAddr.c_str(), &addr.sin_addr);

        if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr),
                   sizeof(addr)) < 0)
        {
            ::close(fd);
            return -1;
        }

        if (::listen(fd, 4) < 0)
        {
            ::close(fd);
            return -1;
        }

        // Set non-blocking.
        const int flags{::fcntl(fd, F_GETFL, 0)};
        if (flags >= 0)
        {
            (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        return fd;
    }

    /// Process one UDS request from raw bytes. Returns response bytes.
    std::vector<std::uint8_t> ProcessUdsRequest(
        ara::diag::DiagnosticManager &diagMgr,
        const std::vector<std::uint8_t> &request)
    {
        if (request.empty())
        {
            // Empty request — negative response generalReject.
            return {0x7FU, 0x00U, 0x10U};
        }

        const std::uint8_t serviceId{request[0]};
        const std::uint16_t subFunc{
            request.size() > 1U
                ? static_cast<std::uint16_t>(request[1])
                : std::uint16_t{0U}};

        auto submitResult{
            diagMgr.SubmitRequest(serviceId, subFunc, request)};
        if (!submitResult.HasValue())
        {
            // Service not registered → negative response serviceNotSupported.
            return {0x7FU, serviceId, 0x11U};
        }

        auto completeResult{
            diagMgr.CompleteRequest(submitResult.Value())};
        if (completeResult.HasValue())
        {
            // Positive response: service ID + 0x40.
            return {static_cast<std::uint8_t>(serviceId + 0x40U), 0x00U};
        }

        // Fallback: conditions not correct.
        return {0x7FU, serviceId, 0x22U};
    }

    void WriteStatus(
        const std::string &statusFile,
        bool listening,
        std::size_t totalRequests,
        std::size_t totalConnections,
        std::size_t activeConnections,
        std::size_t positiveResponses,
        std::size_t negativeResponses)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "listening=" << (listening ? "true" : "false") << "\n";
        stream << "total_requests=" << totalRequests << "\n";
        stream << "total_connections=" << totalConnections << "\n";
        stream << "active_connections=" << activeConnections << "\n";
        stream << "positive_responses=" << positiveResponses << "\n";
        stream << "negative_responses=" << negativeResponses << "\n";
        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string listenAddr{
        GetEnvOrDefault("AUTOSAR_DIAG_LISTEN_ADDR", "0.0.0.0")};
    const std::uint32_t listenPort{
        GetEnvU32("AUTOSAR_DIAG_LISTEN_PORT", 13400U)};
    const std::uint32_t p2ServerMs{
        GetEnvU32("AUTOSAR_DIAG_P2_SERVER_MS", 50U)};
    const std::uint32_t p2StarServerMs{
        GetEnvU32("AUTOSAR_DIAG_P2STAR_SERVER_MS", 5000U)};
    const std::uint32_t statusPeriodMs{
        GetEnvU32("AUTOSAR_DIAG_STATUS_PERIOD_MS", 2000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_DIAG_STATUS_FILE",
            "/run/autosar/diag_server.status")};

    EnsureRunDirectory();

    // Initialize ara::core runtime.
    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        return 1;
    }

    // Configure diagnostic manager.
    ara::diag::DiagnosticManager diagMgr;
    diagMgr.SetResponseTiming({
        static_cast<std::uint16_t>(p2ServerMs),
        static_cast<std::uint16_t>(p2StarServerMs)});

    // Register standard UDS services.
    const std::uint8_t kStandardServices[]{
        0x10U, // DiagnosticSessionControl
        0x11U, // ECUReset
        0x14U, // ClearDTCInformation
        0x19U, // ReadDTCInformation
        0x22U, // ReadDataByIdentifier
        0x27U, // SecurityAccess
        0x2EU, // WriteDataByIdentifier
        0x2FU, // InputOutputControl
        0x31U, // RoutineControl
        0x34U, // RequestDownload
        0x35U, // RequestUpload
        0x36U, // TransferData
        0x37U, // RequestTransferExit
        0x3EU, // TesterPresent
        0x85U  // ControlDTCSetting
    };
    for (auto svc : kStandardServices)
    {
        (void)diagMgr.RegisterService(svc);
    }

    diagMgr.SetResponsePendingCallback(
        [](std::uint32_t, std::uint8_t)
        {
            // ResponsePending (NRC 0x78) tracking — logged via DLT in production.
        });

    // Open DoIP TCP server socket.
    int serverFd{OpenListenSocket(
        listenAddr,
        static_cast<std::uint16_t>(listenPort))};
    const bool listening{serverFd >= 0};

    std::size_t totalRequests{0U};
    std::size_t totalConnections{0U};
    std::size_t positiveResponses{0U};
    std::size_t negativeResponses{0U};
    std::uint64_t lastStatusWriteMs{0U};

    // Track active client connections (up to 4 concurrent).
    std::vector<int> clientFds;
    clientFds.reserve(4U);

    std::vector<std::uint8_t> recvBuffer(4096U);

    while (gRunning.load())
    {
        bool activity{false};

        // Accept new connections.
        if (serverFd >= 0)
        {
            struct sockaddr_in clientAddr {};
            socklen_t addrLen{sizeof(clientAddr)};
            int clientFd{::accept(serverFd,
                                  reinterpret_cast<struct sockaddr *>(&clientAddr),
                                  &addrLen)};
            if (clientFd >= 0)
            {
                // Set non-blocking on client socket.
                const int flags{::fcntl(clientFd, F_GETFL, 0)};
                if (flags >= 0)
                {
                    (void)::fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                }
                clientFds.push_back(clientFd);
                ++totalConnections;
                activity = true;
            }
        }

        // Process data from each client.
        for (auto it = clientFds.begin(); it != clientFds.end();)
        {
            const ssize_t received{
                ::recv(*it, recvBuffer.data(), recvBuffer.size(), 0)};

            if (received > 0)
            {
                activity = true;
                ++totalRequests;

                const std::vector<std::uint8_t> request(
                    recvBuffer.data(),
                    recvBuffer.data() + received);
                const auto response{ProcessUdsRequest(diagMgr, request)};

                if (!response.empty() && response[0] == 0x7FU)
                {
                    ++negativeResponses;
                }
                else
                {
                    ++positiveResponses;
                }

                // Send response.
                (void)::send(*it, response.data(), response.size(), 0);
            }
            else if (received == 0)
            {
                // Client disconnected.
                ::close(*it);
                it = clientFds.erase(it);
                continue;
            }

            ++it;
        }

        // Check timing constraints.
        diagMgr.CheckTimingConstraints();

        // Periodically write status.
        const std::uint64_t nowMs{NowEpochMs()};
        if (nowMs - lastStatusWriteMs >= statusPeriodMs || activity)
        {
            WriteStatus(statusFile,
                        listening,
                        totalRequests,
                        totalConnections,
                        clientFds.size(),
                        positiveResponses,
                        negativeResponses);
            lastStatusWriteMs = nowMs;
        }

        if (!activity && serverFd >= 0)
        {
            struct pollfd pfd;
            pfd.fd = serverFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            ::poll(&pfd, 1, 50);
        }
    }

    // Close all client connections.
    for (int fd : clientFds)
    {
        ::close(fd);
    }
    if (serverFd >= 0)
    {
        ::close(serverFd);
    }

    WriteStatus(statusFile, false, totalRequests, totalConnections,
                0U, positiveResponses, negativeResponses);

    (void)ara::core::Deinitialize();
    return 0;
}
