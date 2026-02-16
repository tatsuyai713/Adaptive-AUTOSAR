/// @file src/main_dlt_daemon.cpp
/// @brief Resident daemon that collects AUTOSAR DLT (Diagnostic Log and
///        Trace) messages from platform and user processes via a local UDP
///        socket, writes them to a DLT log file and optionally forwards
///        them to a remote DLT viewer.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
            if (parsed == 0U || parsed > 600000U)
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

    bool GetEnvBool(const char *key, bool fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        const std::string text{value};
        if (text == "1" || text == "true" || text == "TRUE" || text == "on")
        {
            return true;
        }
        if (text == "0" || text == "false" || text == "FALSE" || text == "off")
        {
            return false;
        }

        return fallback;
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

    void EnsureDirForFile(const std::string &filepath)
    {
        const auto pos{filepath.rfind('/')};
        if (pos == std::string::npos || pos == 0U)
        {
            return;
        }

        const std::string dir{filepath.substr(0U, pos)};
        std::string partial;
        if (dir.front() == '/')
        {
            partial = "/";
        }

        std::size_t begin{0U};
        while (begin < dir.size())
        {
            const auto delim{dir.find('/', begin)};
            const std::size_t len{
                (delim == std::string::npos)
                    ? dir.size() - begin
                    : delim - begin};
            const std::string segment{dir.substr(begin, len)};
            if (!segment.empty())
            {
                if (!partial.empty() && partial.back() != '/')
                {
                    partial.push_back('/');
                }

                partial += segment;
                ::mkdir(partial.c_str(), 0755);
            }

            if (delim == std::string::npos)
            {
                break;
            }
            begin = delim + 1U;
        }
    }

    /// Open a non-blocking UDP server socket for receiving DLT messages.
    int OpenListenSocket(const std::string &listenAddr,
                         std::uint16_t listenPort)
    {
        int fd{::socket(AF_INET, SOCK_DGRAM, 0)};
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

        // Set non-blocking.
        const int flags{::fcntl(fd, F_GETFL, 0)};
        if (flags >= 0)
        {
            (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        return fd;
    }

    /// Open a UDP socket for forwarding DLT messages to a remote host.
    int OpenForwardSocket()
    {
        return ::socket(AF_INET, SOCK_DGRAM, 0);
    }

    /// Rotate the DLT log file when it exceeds the given size.
    void RotateLogFileIfNeeded(
        const std::string &logFilePath,
        std::size_t maxBytes,
        std::size_t maxRotated)
    {
        struct stat st {};
        if (::stat(logFilePath.c_str(), &st) != 0)
        {
            return;
        }

        if (static_cast<std::size_t>(st.st_size) < maxBytes)
        {
            return;
        }

        // Rotate: .dlt.N -> .dlt.N+1, delete oldest.
        for (std::size_t i = maxRotated; i > 0U; --i)
        {
            const std::string from{
                logFilePath + "." + std::to_string(i - 1U)};
            const std::string to{
                logFilePath + "." + std::to_string(i)};

            if (i == maxRotated)
            {
                ::remove(to.c_str());
            }

            ::rename(from.c_str(), to.c_str());
        }

        ::rename(logFilePath.c_str(),
                 (logFilePath + ".0").c_str());
    }

    void WriteStatus(
        const std::string &statusFile,
        std::size_t messagesReceived,
        std::size_t bytesReceived,
        std::size_t messagesForwarded,
        std::size_t forwardErrors,
        std::size_t fileWrites,
        bool listening)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "listening=" << (listening ? "true" : "false") << "\n";
        stream << "messages_received=" << messagesReceived << "\n";
        stream << "bytes_received=" << bytesReceived << "\n";
        stream << "messages_forwarded=" << messagesForwarded << "\n";
        stream << "forward_errors=" << forwardErrors << "\n";
        stream << "file_writes=" << fileWrites << "\n";
        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string listenAddr{
        GetEnvOrDefault("AUTOSAR_DLT_LISTEN_ADDR", "0.0.0.0")};
    const std::uint32_t listenPort{
        GetEnvU32("AUTOSAR_DLT_LISTEN_PORT", 3490U)};
    const std::string logFilePath{
        GetEnvOrDefault(
            "AUTOSAR_DLT_LOG_FILE",
            "/var/log/autosar/dlt.log")};
    const std::uint32_t maxFileSizeKb{
        GetEnvU32("AUTOSAR_DLT_MAX_FILE_SIZE_KB", 10240U)};
    const std::uint32_t maxRotated{
        GetEnvU32("AUTOSAR_DLT_MAX_ROTATED_FILES", 5U)};
    const bool forwardEnabled{
        GetEnvBool("AUTOSAR_DLT_FORWARD_ENABLED", false)};
    const std::string forwardHost{
        GetEnvOrDefault("AUTOSAR_DLT_FORWARD_HOST", "192.168.1.100")};
    const std::uint32_t forwardPort{
        GetEnvU32("AUTOSAR_DLT_FORWARD_PORT", 3490U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_DLT_STATUS_FILE",
            "/run/autosar/dlt_daemon.status")};
    const std::uint32_t statusPeriodMs{
        GetEnvU32("AUTOSAR_DLT_STATUS_PERIOD_MS", 2000U)};

    EnsureRunDirectory();
    EnsureDirForFile(logFilePath);

    int listenFd{OpenListenSocket(
        listenAddr,
        static_cast<std::uint16_t>(listenPort))};
    const bool listening{listenFd >= 0};

    int forwardFd{-1};
    struct sockaddr_in forwardAddr {};
    if (forwardEnabled)
    {
        forwardFd = OpenForwardSocket();
        if (forwardFd >= 0)
        {
            forwardAddr.sin_family = AF_INET;
            forwardAddr.sin_port = htons(
                static_cast<std::uint16_t>(forwardPort));
            ::inet_pton(AF_INET, forwardHost.c_str(),
                        &forwardAddr.sin_addr);
        }
    }

    std::size_t messagesReceived{0U};
    std::size_t bytesReceived{0U};
    std::size_t messagesForwarded{0U};
    std::size_t forwardErrors{0U};
    std::size_t fileWrites{0U};
    std::uint64_t lastStatusWriteMs{0U};

    std::vector<std::uint8_t> recvBuffer(65536U);

    while (gRunning.load())
    {
        bool activity{false};

        if (listenFd >= 0)
        {
            for (int batch = 0; batch < 64; ++batch)
            {
                const ssize_t received{
                    ::recvfrom(listenFd, recvBuffer.data(),
                               recvBuffer.size(), 0,
                               nullptr, nullptr)};
                if (received <= 0)
                {
                    break;
                }

                activity = true;
                ++messagesReceived;
                bytesReceived += static_cast<std::size_t>(received);

                // Write raw DLT bytes to log file.
                RotateLogFileIfNeeded(
                    logFilePath,
                    static_cast<std::size_t>(maxFileSizeKb) * 1024U,
                    static_cast<std::size_t>(maxRotated));

                std::ofstream logFile(
                    logFilePath,
                    std::ios::binary | std::ios::app);
                if (logFile.is_open())
                {
                    logFile.write(
                        reinterpret_cast<const char *>(recvBuffer.data()),
                        received);
                    ++fileWrites;
                }

                // Forward to remote if configured.
                if (forwardFd >= 0 && forwardEnabled)
                {
                    const ssize_t sent{
                        ::sendto(
                            forwardFd,
                            recvBuffer.data(),
                            static_cast<std::size_t>(received),
                            0,
                            reinterpret_cast<const struct sockaddr *>(
                                &forwardAddr),
                            sizeof(forwardAddr))};

                    if (sent > 0)
                    {
                        ++messagesForwarded;
                    }
                    else
                    {
                        ++forwardErrors;
                    }
                }
            }
        }

        // Periodically write status.
        const std::uint64_t nowMs{NowEpochMs()};
        if (nowMs - lastStatusWriteMs >= statusPeriodMs || activity)
        {
            WriteStatus(statusFile,
                        messagesReceived,
                        bytesReceived,
                        messagesForwarded,
                        forwardErrors,
                        fileWrites,
                        listening);
            lastStatusWriteMs = nowMs;
        }

        // Small sleep to avoid busy-wait when no data arrives.
        if (!activity)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10U));
        }
    }

    WriteStatus(statusFile,
                messagesReceived,
                bytesReceived,
                messagesForwarded,
                forwardErrors,
                fileWrites,
                listening);

    if (listenFd >= 0)
    {
        ::close(listenFd);
    }
    if (forwardFd >= 0)
    {
        ::close(forwardFd);
    }

    return 0;
}
