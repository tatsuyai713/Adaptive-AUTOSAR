// This executable is a Linux/Ubuntu-side diagnostic tester for Raspberry Pi ECU.
// It speaks DoIP over UDP/TCP and sends UDS payloads for data read and communication tests.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    // DoIP payload types used by this tester.
    constexpr std::uint16_t kDoipPayloadGenericNack{0x0000U};
    constexpr std::uint16_t kDoipPayloadVehicleIdRequest{0x0001U};
    constexpr std::uint16_t kDoipPayloadVehicleIdResponse{0x0004U};
    constexpr std::uint16_t kDoipPayloadRoutingActivationRequest{0x0005U};
    constexpr std::uint16_t kDoipPayloadRoutingActivationResponse{0x0006U};
    constexpr std::uint16_t kDoipPayloadAliveCheckRequest{0x0007U};
    constexpr std::uint16_t kDoipPayloadAliveCheckResponse{0x0008U};
    constexpr std::uint16_t kDoipPayloadDiagMessage{0x8001U};
    constexpr std::uint16_t kDoipPayloadDiagAck{0x8002U};
    constexpr std::uint16_t kDoipPayloadDiagNack{0x8003U};

    // Routing activation response codes.
    constexpr std::uint8_t kRoutingActivationSuccess{0x10U};
    constexpr std::uint8_t kRoutingActivationPending{0x11U};

    // Minimal DoIP header size and payload guard to avoid accidental oversized allocations.
    constexpr std::size_t kDoipHeaderSize{8U};
    constexpr std::size_t kDoipMaxPayloadSize{1024U * 1024U};

    enum class Mode
    {
        VehicleId,
        RoutingActivation,
        DiagReadDid,
        DiagCustom,
        TxTest,
        RxTest,
        FullTest
    };

    struct Config
    {
        Mode mode{Mode::FullTest};
        std::string host{"127.0.0.1"};
        std::uint16_t tcpPort{8081U};
        std::uint16_t udpPort{8081U};
        std::uint8_t protocolVersion{0x02U};
        std::uint16_t testerAddress{0x0E80U};
        std::uint16_t targetAddress{0x0001U};
        std::uint8_t activationType{0x00U};
        std::uint16_t did{0xF50DU};
        std::string udsHex;
        std::size_t count{20U};
        std::size_t minRx{0U};
        std::uint32_t intervalMs{100U};
        std::uint32_t timeoutMs{2000U};
        std::size_t fixedPacketSize{64U};
        bool udpBroadcast{false};
        bool vehicleIdUseTcp{true};
        bool autoTargetFromRouting{true};
        bool routingActivationOptional{true};
        bool requestVehicleIdInFull{false};
    };

    struct DoipFrame
    {
        std::uint8_t protocolVersion{0U};
        std::uint8_t inverseProtocolVersion{0U};
        std::uint16_t payloadType{0U};
        std::vector<std::uint8_t> payload;
    };

    struct VehicleAnnouncement
    {
        std::string vin;
        std::uint16_t logicalAddress{0U};
        std::vector<std::uint8_t> eid;
        std::vector<std::uint8_t> gid;
        std::uint8_t furtherAction{0U};
        bool hasVinGidStatus{false};
        std::uint8_t vinGidStatus{0U};
    };

    struct RoutingActivationResult
    {
        std::uint16_t testerAddress{0U};
        std::uint16_t entityAddress{0U};
        std::uint8_t responseCode{0U};
    };

    struct DiagResponse
    {
        bool isPositiveAck{false};
        std::uint8_t code{0U};
        std::vector<std::uint8_t> udsPayload;
    };

    std::string HexByte(std::uint8_t value)
    {
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(value);
        return stream.str();
    }

    std::string HexU16(std::uint16_t value)
    {
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<unsigned int>(value);
        return stream.str();
    }

    std::string ToHex(const std::vector<std::uint8_t> &bytes)
    {
        std::ostringstream stream;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            if (i > 0U)
            {
                stream << ' ';
            }
            stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned int>(bytes[i]);
        }
        return stream.str();
    }

    std::uint16_t ReadU16Be(const std::vector<std::uint8_t> &bytes, std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes.at(offset)) << 8U) |
            static_cast<std::uint16_t>(bytes.at(offset + 1U)));
    }

    std::uint32_t ReadU32Be(const std::vector<std::uint8_t> &bytes, std::size_t offset)
    {
        return (static_cast<std::uint32_t>(bytes.at(offset)) << 24U) |
               (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 16U) |
               (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 8U) |
               static_cast<std::uint32_t>(bytes.at(offset + 3U));
    }

    void AppendU16Be(std::vector<std::uint8_t> &bytes, std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    void AppendU32Be(std::vector<std::uint8_t> &bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    }

    bool ParseUnsigned(
        const std::string &text,
        std::uint64_t maxValue,
        std::uint64_t &value)
    {
        try
        {
            std::size_t consumed{0U};
            const std::uint64_t parsed{std::stoull(text, &consumed, 0)};
            if (consumed != text.size() || parsed > maxValue)
            {
                return false;
            }
            value = parsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseBool(const std::string &text, bool &value)
    {
        std::string lowered{text};
        std::transform(
            lowered.begin(),
            lowered.end(),
            lowered.begin(),
            [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on")
        {
            value = true;
            return true;
        }

        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off")
        {
            value = false;
            return true;
        }

        return false;
    }

    bool ParseHexByteToken(const std::string &token, std::uint8_t &value)
    {
        std::string cleaned{token};
        if (cleaned.size() >= 2U && cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X'))
        {
            cleaned = cleaned.substr(2U);
        }

        if (cleaned.empty() || cleaned.size() > 2U)
        {
            return false;
        }

        std::uint64_t parsed{0U};
        if (!ParseUnsigned("0x" + cleaned, 0xFFU, parsed))
        {
            return false;
        }

        value = static_cast<std::uint8_t>(parsed);
        return true;
    }

    bool ParseUdsHex(const std::string &text, std::vector<std::uint8_t> &out)
    {
        out.clear();

        // If separators exist, parse token by token, e.g. "22 F5 0D" or "0x22,0xF5,0x0D".
        if (text.find_first_of(" ,;:\t") != std::string::npos)
        {
            std::string normalized{text};
            for (auto &ch : normalized)
            {
                if (ch == ',' || ch == ';' || ch == ':')
                {
                    ch = ' ';
                }
            }

            std::stringstream parser(normalized);
            std::string token;
            while (parser >> token)
            {
                std::uint8_t byte{0U};
                if (!ParseHexByteToken(token, byte))
                {
                    return false;
                }
                out.push_back(byte);
            }
            return !out.empty();
        }

        // Without separators, parse as concatenated hex bytes, e.g. "22F50D".
        std::string compact{text};
        if (compact.size() >= 2U && compact[0] == '0' && (compact[1] == 'x' || compact[1] == 'X'))
        {
            compact = compact.substr(2U);
        }

        if (compact.empty() || (compact.size() % 2U) != 0U)
        {
            return false;
        }

        for (std::size_t i = 0U; i < compact.size(); i += 2U)
        {
            const std::string token{compact.substr(i, 2U)};
            std::uint8_t byte{0U};
            if (!ParseHexByteToken(token, byte))
            {
                return false;
            }
            out.push_back(byte);
        }

        return true;
    }

    std::vector<std::uint8_t> BuildDoipPacket(
        std::uint8_t protocolVersion,
        std::uint16_t payloadType,
        const std::vector<std::uint8_t> &payload)
    {
        std::vector<std::uint8_t> packet;
        packet.reserve(kDoipHeaderSize + payload.size());

        packet.push_back(protocolVersion);
        packet.push_back(static_cast<std::uint8_t>(~protocolVersion));
        AppendU16Be(packet, payloadType);
        AppendU32Be(packet, static_cast<std::uint32_t>(payload.size()));
        packet.insert(packet.end(), payload.begin(), payload.end());

        return packet;
    }

    bool ParseDoipPacket(
        const std::vector<std::uint8_t> &packet,
        DoipFrame &frame,
        std::string &error)
    {
        if (packet.size() < kDoipHeaderSize)
        {
            error = "packet is shorter than DoIP header";
            return false;
        }

        const std::uint8_t version{packet[0U]};
        const std::uint8_t inverse{packet[1U]};
        if (static_cast<std::uint8_t>(~version) != inverse)
        {
            error = "protocol/inverse-protocol mismatch";
            return false;
        }

        const std::uint16_t payloadType{ReadU16Be(packet, 2U)};
        const std::uint32_t payloadLength{ReadU32Be(packet, 4U)};

        if (payloadLength > kDoipMaxPayloadSize)
        {
            error = "payload length is too large";
            return false;
        }

        const std::size_t expectedSize{kDoipHeaderSize + static_cast<std::size_t>(payloadLength)};
        if (packet.size() < expectedSize)
        {
            error = "packet ended before full payload";
            return false;
        }

        frame.protocolVersion = version;
        frame.inverseProtocolVersion = inverse;
        frame.payloadType = payloadType;
        frame.payload.assign(packet.begin() + static_cast<std::ptrdiff_t>(kDoipHeaderSize),
                             packet.begin() + static_cast<std::ptrdiff_t>(expectedSize));

        return true;
    }

    bool PollForEvent(
        int fd,
        short events,
        std::uint32_t timeoutMs,
        std::string &error)
    {
        struct pollfd entry;
        std::memset(&entry, 0, sizeof(entry));
        entry.fd = fd;
        entry.events = events;

        const int rc{::poll(&entry, 1, static_cast<int>(timeoutMs))};
        if (rc == 0)
        {
            error = "operation timeout";
            return false;
        }

        if (rc < 0)
        {
            error = std::string("poll failed: ") + std::strerror(errno);
            return false;
        }

        if ((entry.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
            error = "poll reported socket error/hangup";
            return false;
        }

        return true;
    }

    bool ReadExact(
        int fd,
        std::uint8_t *buffer,
        std::size_t length,
        std::uint32_t timeoutMs,
        std::string &error)
    {
        std::size_t total{0U};
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs)};

        while (total < length)
        {
            const auto now{std::chrono::steady_clock::now()};
            if (now >= deadline)
            {
                error = "read timed out";
                return false;
            }

            const auto remaining{std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()};
            if (!PollForEvent(fd, POLLIN, static_cast<std::uint32_t>(remaining), error))
            {
                return false;
            }

            const ssize_t rc{::recv(fd, buffer + total, length - total, 0)};
            if (rc == 0)
            {
                error = "peer closed TCP connection";
                return false;
            }

            if (rc < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                error = std::string("recv failed: ") + std::strerror(errno);
                return false;
            }

            total += static_cast<std::size_t>(rc);
        }

        return true;
    }

    bool WriteExact(
        int fd,
        const std::uint8_t *buffer,
        std::size_t length,
        std::uint32_t timeoutMs,
        std::string &error)
    {
        std::size_t total{0U};
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs)};

        while (total < length)
        {
            const auto now{std::chrono::steady_clock::now()};
            if (now >= deadline)
            {
                error = "write timed out";
                return false;
            }

            const auto remaining{std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()};
            if (!PollForEvent(fd, POLLOUT, static_cast<std::uint32_t>(remaining), error))
            {
                return false;
            }

            const ssize_t rc{::send(fd, buffer + total, length - total, 0)};
            if (rc < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                error = std::string("send failed: ") + std::strerror(errno);
                return false;
            }

            total += static_cast<std::size_t>(rc);
        }

        return true;
    }

    int ConnectTcp(const std::string &host, std::uint16_t port, std::uint32_t timeoutMs, std::string &error)
    {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *result{nullptr};
        const std::string portText{std::to_string(port)};
        const int gai{::getaddrinfo(host.c_str(), portText.c_str(), &hints, &result)};
        if (gai != 0)
        {
            error = std::string("getaddrinfo failed: ") + ::gai_strerror(gai);
            return -1;
        }

        int connectedFd{-1};

        for (struct addrinfo *entry = result; entry != nullptr; entry = entry->ai_next)
        {
            const int fd{::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol)};
            if (fd < 0)
            {
                continue;
            }

            const int flags{::fcntl(fd, F_GETFL, 0)};
            if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
            {
                ::close(fd);
                continue;
            }

            int rc{::connect(fd, entry->ai_addr, static_cast<socklen_t>(entry->ai_addrlen))};
            if (rc < 0 && errno != EINPROGRESS)
            {
                ::close(fd);
                continue;
            }

            std::string pollError;
            if (!PollForEvent(fd, POLLOUT, timeoutMs, pollError))
            {
                ::close(fd);
                error = "tcp connect timeout/error: " + pollError;
                continue;
            }

            int soError{0};
            socklen_t soErrorLen{static_cast<socklen_t>(sizeof(soError))};
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &soErrorLen) != 0 || soError != 0)
            {
                ::close(fd);
                continue;
            }

            // Return to blocking mode; I/O timeouts are handled by poll in read/write helpers.
            (void)::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
            connectedFd = fd;
            break;
        }

        ::freeaddrinfo(result);

        if (connectedFd < 0)
        {
            if (error.empty())
            {
                error = "failed to connect TCP socket";
            }
            return -1;
        }

        return connectedFd;
    }

    bool SendTcpFrame(
        int fd,
        std::uint8_t protocolVersion,
        std::uint16_t payloadType,
        const std::vector<std::uint8_t> &payload,
        std::size_t fixedPacketSize,
        std::uint32_t timeoutMs,
        std::string &error)
    {
        auto packet{BuildDoipPacket(protocolVersion, payloadType, payload)};
        if (fixedPacketSize > 0U)
        {
            if (packet.size() > fixedPacketSize)
            {
                error = "packet exceeds configured fixed-packet-size";
                return false;
            }
            packet.resize(fixedPacketSize, 0U);
        }

        return WriteExact(fd, packet.data(), packet.size(), timeoutMs, error);
    }

    bool ReceiveTcpFrame(
        int fd,
        std::size_t fixedPacketSize,
        std::uint32_t timeoutMs,
        DoipFrame &frame,
        std::string &error)
    {
        std::vector<std::uint8_t> header(kDoipHeaderSize, 0U);
        if (!ReadExact(fd, header.data(), header.size(), timeoutMs, error))
        {
            return false;
        }

        const std::uint32_t payloadLength{ReadU32Be(header, 4U)};
        if (payloadLength > kDoipMaxPayloadSize)
        {
            error = "incoming payload length is too large";
            return false;
        }

        if (fixedPacketSize > 0U)
        {
            const std::size_t expectedFrameSize{
                kDoipHeaderSize + static_cast<std::size_t>(payloadLength)};
            if (expectedFrameSize > fixedPacketSize)
            {
                error = "incoming payload is larger than fixed-packet-size";
                return false;
            }
        }

        std::vector<std::uint8_t> packet;
        packet.reserve(kDoipHeaderSize + static_cast<std::size_t>(payloadLength));
        packet.insert(packet.end(), header.begin(), header.end());

        if (payloadLength > 0U)
        {
            const std::size_t baseSize{packet.size()};
            packet.resize(baseSize + static_cast<std::size_t>(payloadLength));
            if (!ReadExact(fd,
                           packet.data() + static_cast<std::ptrdiff_t>(baseSize),
                           static_cast<std::size_t>(payloadLength),
                           timeoutMs,
                           error))
            {
                return false;
            }
        }

        if (fixedPacketSize > 0U)
        {
            const std::size_t consumedBytes{
                kDoipHeaderSize + static_cast<std::size_t>(payloadLength)};
            if (consumedBytes < fixedPacketSize)
            {
                std::vector<std::uint8_t> padding(fixedPacketSize - consumedBytes, 0U);
                if (!ReadExact(fd, padding.data(), padding.size(), timeoutMs, error))
                {
                    return false;
                }
            }
        }

        return ParseDoipPacket(packet, frame, error);
    }

    bool WaitForOneOfPayloadTypes(
        int fd,
        const std::vector<std::uint16_t> &acceptedTypes,
        std::uint16_t aliveCheckSourceAddress,
        std::size_t fixedPacketSize,
        std::uint32_t timeoutMs,
        DoipFrame &frame,
        std::string &error)
    {
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs)};

        while (true)
        {
            const auto now{std::chrono::steady_clock::now()};
            if (now >= deadline)
            {
                error = "timeout while waiting for expected DoIP payload type";
                return false;
            }

            const auto remaining{std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()};
            DoipFrame candidate;
            if (!ReceiveTcpFrame(
                    fd,
                    fixedPacketSize,
                    static_cast<std::uint32_t>(remaining),
                    candidate,
                    error))
            {
                return false;
            }

            if (candidate.payloadType == kDoipPayloadAliveCheckRequest)
            {
                // Respond to AliveCheck to keep the DoIP session valid.
                std::vector<std::uint8_t> alivePayload;
                alivePayload.reserve(2U);
                AppendU16Be(alivePayload, aliveCheckSourceAddress);
                std::string ignoredError;
                (void)SendTcpFrame(
                    fd,
                    candidate.protocolVersion,
                    kDoipPayloadAliveCheckResponse,
                    alivePayload,
                    fixedPacketSize,
                    300U,
                    ignoredError);
                continue;
            }

            if (std::find(acceptedTypes.begin(), acceptedTypes.end(), candidate.payloadType) != acceptedTypes.end())
            {
                frame = std::move(candidate);
                return true;
            }
        }
    }

    bool ParseVehicleAnnouncement(
        const std::vector<std::uint8_t> &payload,
        VehicleAnnouncement &info,
        std::string &error)
    {
        if (payload.size() != 32U && payload.size() != 33U)
        {
            error = "vehicle announcement payload must be 32 or 33 bytes";
            return false;
        }

        info.vin.assign(payload.begin(), payload.begin() + 17);
        for (auto &ch : info.vin)
        {
            if (!std::isprint(static_cast<unsigned char>(ch)))
            {
                ch = '.';
            }
        }

        info.logicalAddress = ReadU16Be(payload, 17U);
        info.eid.assign(payload.begin() + 19, payload.begin() + 25);
        info.gid.assign(payload.begin() + 25, payload.begin() + 31);
        info.furtherAction = payload[31U];

        if (payload.size() == 33U)
        {
            info.hasVinGidStatus = true;
            info.vinGidStatus = payload[32U];
        }

        return true;
    }

    bool ParseRoutingActivationResponse(
        const std::vector<std::uint8_t> &payload,
        RoutingActivationResult &result,
        std::string &error)
    {
        if (payload.size() != 9U && payload.size() != 13U)
        {
            error = "routing activation response payload must be 9 or 13 bytes";
            return false;
        }

        result.testerAddress = ReadU16Be(payload, 0U);
        result.entityAddress = ReadU16Be(payload, 2U);
        result.responseCode = payload[4U];
        return true;
    }

    bool ParseDiagPayload(
        const DoipFrame &frame,
        DiagResponse &response,
        std::string &error)
    {
        if (frame.payloadType != kDoipPayloadDiagAck && frame.payloadType != kDoipPayloadDiagNack)
        {
            error = "unexpected diagnostic payload type";
            return false;
        }

        if (frame.payload.size() < 5U)
        {
            error = "diagnostic response payload too short";
            return false;
        }

        response.isPositiveAck = (frame.payloadType == kDoipPayloadDiagAck);
        response.code = frame.payload[4U];
        response.udsPayload.assign(frame.payload.begin() + 5, frame.payload.end());
        return true;
    }

    std::string RoutingActivationCodeToString(std::uint8_t code)
    {
        switch (code)
        {
        case 0x00:
            return "InvalidSourceAddress";
        case 0x01:
            return "NoSocketAvailable";
        case 0x02:
            return "Busy";
        case 0x03:
            return "AlreadyRegisteredTester";
        case 0x04:
            return "FailedAuthentication";
        case 0x05:
            return "RejectedConfirmation";
        case 0x06:
            return "UnsupportedActivationType";
        case 0x07:
            return "NoSecureSocket";
        case 0x10:
            return "Successful";
        case 0x11:
            return "Pending";
        default:
            return "Unknown";
        }
    }

    std::vector<std::uint8_t> BuildRoutingActivationRequest(
        std::uint16_t testerAddress,
        std::uint8_t activationType)
    {
        std::vector<std::uint8_t> payload;
        payload.reserve(7U);
        AppendU16Be(payload, testerAddress);
        payload.push_back(activationType);
        AppendU32Be(payload, 0U);
        return payload;
    }

    std::vector<std::uint8_t> BuildDiagPayload(
        std::uint16_t testerAddress,
        std::uint16_t targetAddress,
        const std::vector<std::uint8_t> &uds)
    {
        std::vector<std::uint8_t> payload;
        payload.reserve(4U + uds.size());
        AppendU16Be(payload, testerAddress);
        AppendU16Be(payload, targetAddress);
        payload.insert(payload.end(), uds.begin(), uds.end());
        return payload;
    }

    std::vector<std::uint8_t> BuildReadDidRequest(std::uint16_t did)
    {
        std::vector<std::uint8_t> uds;
        uds.reserve(3U);
        uds.push_back(0x22U);
        uds.push_back(static_cast<std::uint8_t>((did >> 8U) & 0xFFU));
        uds.push_back(static_cast<std::uint8_t>(did & 0xFFU));
        return uds;
    }

    void PrintUsage(const char *program)
    {
        std::cout
            << "DoIP/DIAG Ubuntu Tester for Raspberry Pi ECU\n"
            << "Usage:\n"
            << "  " << program << " --mode=<vehicle-id|routing-activation|diag-read-did|diag-custom|tx-test|rx-test|full-test> [options]\n\n"
            << "Key options:\n"
            << "  --host=<ip-or-hostname>          Default: 127.0.0.1\n"
            << "  --tcp-port=<port>                Default: 8081 (repo default DoIP TCP port)\n"
            << "  --udp-port=<port>                Default: 8081\n"
            << "  --protocol-version=<num>         Default: 0x02\n"
            << "  --tester-address=<hex>           Default: 0x0E80\n"
            << "  --target-address=<hex>           Default: 0x0001\n"
            << "  --activation-type=<hex>          Default: 0x00\n"
            << "  --did=<hex>                      Default: 0xF50D\n"
            << "  --uds=<hex-bytes>                Example: 22F50D or \"22 F5 0D\"\n"
            << "  --count=<n>                      Default: 20\n"
            << "  --min-rx=<n>                     Default: 0\n"
            << "  --interval-ms=<n>                Default: 100\n"
            << "  --timeout-ms=<n>                 Default: 2000\n"
            << "  --fixed-packet-size=<n>          Default: 64 (set 0 for variable-size DoIP framing)\n"
            << "  --vehicle-id-transport=<tcp|udp> Default: tcp\n"
            << "  --udp-broadcast                  Send Vehicle-ID request as UDP broadcast\n"
            << "  --auto-target-from-routing=<b>   Default: true\n"
            << "  --routing-activation-optional=<b> Default: true (diag/test modes)\n"
            << "  --request-vehicle-id-in-full=<b> Default: false\n"
            << "\nExamples:\n"
            << "  " << program << " --mode=vehicle-id --host=192.168.10.20 --udp-port=8081\n"
            << "  " << program << " --mode=routing-activation --host=192.168.10.20 --tcp-port=8081\n"
            << "  " << program << " --mode=diag-read-did --host=192.168.10.20 --did=0xF50D\n"
            << "  " << program << " --mode=diag-custom --host=192.168.10.20 --uds=22F52F\n"
            << "  " << program << " --mode=tx-test --host=192.168.10.20 --did=0xF505 --count=100\n"
            << "  " << program << " --mode=rx-test --host=192.168.10.20 --did=0xF5A6 --count=100 --min-rx=90\n"
            << "\nKnown DIDs on this repository's ECU sample:\n"
            << "  0xF50D AverageSpeed, 0xF52F FuelAmount, 0xF546 ExternalTemperature,\n"
            << "  0xF55E AverageFuelConsumption, 0xF505 EngineCoolantTemperature, 0xF5A6 Odometer\n";
    }

    bool ParseMode(const std::string &text, Mode &mode)
    {
        if (text == "vehicle-id")
        {
            mode = Mode::VehicleId;
            return true;
        }
        if (text == "routing-activation")
        {
            mode = Mode::RoutingActivation;
            return true;
        }
        if (text == "diag-read-did")
        {
            mode = Mode::DiagReadDid;
            return true;
        }
        if (text == "diag-custom")
        {
            mode = Mode::DiagCustom;
            return true;
        }
        if (text == "tx-test")
        {
            mode = Mode::TxTest;
            return true;
        }
        if (text == "rx-test")
        {
            mode = Mode::RxTest;
            return true;
        }
        if (text == "full-test")
        {
            mode = Mode::FullTest;
            return true;
        }
        return false;
    }

    bool ParseArgKeyValue(const std::string &arg, std::string &key, std::string &value)
    {
        if (arg.size() < 3U || arg[0] != '-' || arg[1] != '-')
        {
            return false;
        }

        const std::size_t eqPos{arg.find('=')};
        if (eqPos == std::string::npos)
        {
            key = arg.substr(2U);
            value.clear();
            return true;
        }

        key = arg.substr(2U, eqPos - 2U);
        value = arg.substr(eqPos + 1U);
        return true;
    }

    bool ParseArgs(int argc, char **argv, Config &config)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg{argv[i]};

            if (arg == "--help" || arg == "-h")
            {
                PrintUsage(argv[0]);
                return false;
            }

            std::string key;
            std::string value;
            if (!ParseArgKeyValue(arg, key, value))
            {
                std::cerr << "[ERROR] Invalid argument format: " << arg << "\n";
                return false;
            }

            if (key == "udp-broadcast" && value.empty())
            {
                config.udpBroadcast = true;
                continue;
            }

            std::uint64_t numericValue{0U};
            bool boolValue{false};

            if (key == "mode")
            {
                if (!ParseMode(value, config.mode))
                {
                    std::cerr << "[ERROR] Unsupported mode: " << value << "\n";
                    return false;
                }
            }
            else if (key == "host")
            {
                config.host = value;
            }
            else if (key == "tcp-port")
            {
                if (!ParseUnsigned(value, 65535U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid tcp-port value\n";
                    return false;
                }
                config.tcpPort = static_cast<std::uint16_t>(numericValue);
            }
            else if (key == "udp-port")
            {
                if (!ParseUnsigned(value, 65535U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid udp-port value\n";
                    return false;
                }
                config.udpPort = static_cast<std::uint16_t>(numericValue);
            }
            else if (key == "protocol-version")
            {
                if (!ParseUnsigned(value, 0xFFU, numericValue))
                {
                    std::cerr << "[ERROR] Invalid protocol-version value\n";
                    return false;
                }
                config.protocolVersion = static_cast<std::uint8_t>(numericValue);
            }
            else if (key == "tester-address")
            {
                if (!ParseUnsigned(value, 0xFFFFU, numericValue))
                {
                    std::cerr << "[ERROR] Invalid tester-address value\n";
                    return false;
                }
                config.testerAddress = static_cast<std::uint16_t>(numericValue);
            }
            else if (key == "target-address")
            {
                if (!ParseUnsigned(value, 0xFFFFU, numericValue))
                {
                    std::cerr << "[ERROR] Invalid target-address value\n";
                    return false;
                }
                config.targetAddress = static_cast<std::uint16_t>(numericValue);
            }
            else if (key == "activation-type")
            {
                if (!ParseUnsigned(value, 0xFFU, numericValue))
                {
                    std::cerr << "[ERROR] Invalid activation-type value\n";
                    return false;
                }
                config.activationType = static_cast<std::uint8_t>(numericValue);
            }
            else if (key == "did")
            {
                if (!ParseUnsigned(value, 0xFFFFU, numericValue))
                {
                    std::cerr << "[ERROR] Invalid did value\n";
                    return false;
                }
                config.did = static_cast<std::uint16_t>(numericValue);
            }
            else if (key == "uds")
            {
                config.udsHex = value;
            }
            else if (key == "count")
            {
                if (!ParseUnsigned(value, 1000000U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid count value\n";
                    return false;
                }
                config.count = static_cast<std::size_t>(numericValue);
            }
            else if (key == "min-rx")
            {
                if (!ParseUnsigned(value, 1000000U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid min-rx value\n";
                    return false;
                }
                config.minRx = static_cast<std::size_t>(numericValue);
            }
            else if (key == "interval-ms")
            {
                if (!ParseUnsigned(value, 3600000U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid interval-ms value\n";
                    return false;
                }
                config.intervalMs = static_cast<std::uint32_t>(numericValue);
            }
            else if (key == "timeout-ms")
            {
                if (!ParseUnsigned(value, 3600000U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid timeout-ms value\n";
                    return false;
                }
                config.timeoutMs = static_cast<std::uint32_t>(numericValue);
            }
            else if (key == "fixed-packet-size")
            {
                if (!ParseUnsigned(value, 1024U * 1024U, numericValue))
                {
                    std::cerr << "[ERROR] Invalid fixed-packet-size value\n";
                    return false;
                }
                config.fixedPacketSize = static_cast<std::size_t>(numericValue);
            }
            else if (key == "vehicle-id-transport")
            {
                if (value == "tcp")
                {
                    config.vehicleIdUseTcp = true;
                }
                else if (value == "udp")
                {
                    config.vehicleIdUseTcp = false;
                }
                else
                {
                    std::cerr << "[ERROR] vehicle-id-transport must be tcp or udp\n";
                    return false;
                }
            }
            else if (key == "udp-broadcast")
            {
                if (!ParseBool(value, boolValue))
                {
                    std::cerr << "[ERROR] Invalid udp-broadcast value\n";
                    return false;
                }
                config.udpBroadcast = boolValue;
            }
            else if (key == "auto-target-from-routing")
            {
                if (!ParseBool(value, boolValue))
                {
                    std::cerr << "[ERROR] Invalid auto-target-from-routing value\n";
                    return false;
                }
                config.autoTargetFromRouting = boolValue;
            }
            else if (key == "routing-activation-optional")
            {
                if (!ParseBool(value, boolValue))
                {
                    std::cerr << "[ERROR] Invalid routing-activation-optional value\n";
                    return false;
                }
                config.routingActivationOptional = boolValue;
            }
            else if (key == "request-vehicle-id-in-full")
            {
                if (!ParseBool(value, boolValue))
                {
                    std::cerr << "[ERROR] Invalid request-vehicle-id-in-full value\n";
                    return false;
                }
                config.requestVehicleIdInFull = boolValue;
            }
            else
            {
                std::cerr << "[ERROR] Unknown argument: " << arg << "\n";
                return false;
            }
        }

        return true;
    }

    int OpenUdpSocket(
        const std::string &host,
        std::uint16_t port,
        bool broadcast,
        sockaddr_storage &destination,
        socklen_t &destinationLen,
        std::string &error)
    {
        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;

        struct addrinfo *result{nullptr};
        const std::string portText{std::to_string(port)};
        const int gai{::getaddrinfo(host.c_str(), portText.c_str(), &hints, &result)};
        if (gai != 0)
        {
            error = std::string("getaddrinfo failed: ") + ::gai_strerror(gai);
            return -1;
        }

        int fd{-1};
        for (struct addrinfo *entry = result; entry != nullptr; entry = entry->ai_next)
        {
            fd = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
            if (fd < 0)
            {
                continue;
            }

            if (broadcast)
            {
                const int enabled{1};
                if (::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled)) != 0)
                {
                    ::close(fd);
                    fd = -1;
                    continue;
                }
            }

            std::memset(&destination, 0, sizeof(destination));
            std::memcpy(&destination, entry->ai_addr, static_cast<std::size_t>(entry->ai_addrlen));
            destinationLen = static_cast<socklen_t>(entry->ai_addrlen);
            break;
        }

        ::freeaddrinfo(result);

        if (fd < 0)
        {
            error = "failed to create UDP socket";
        }

        return fd;
    }

    bool RunVehicleIdRequestUdp(const Config &config)
    {
        std::string error;
        sockaddr_storage destination;
        socklen_t destinationLen{0U};
        const int fd{OpenUdpSocket(config.host, config.udpPort, config.udpBroadcast, destination, destinationLen, error)};
        if (fd < 0)
        {
            std::cerr << "[ERROR] vehicle-id: " << error << "\n";
            return false;
        }

        std::vector<std::uint8_t> requestPacket{
            BuildDoipPacket(config.protocolVersion, kDoipPayloadVehicleIdRequest, {})};
        if (config.fixedPacketSize > 0U)
        {
            if (requestPacket.size() > config.fixedPacketSize)
            {
                std::cerr << "[ERROR] vehicle-id udp request is larger than fixed-packet-size\n";
                ::close(fd);
                return false;
            }
            requestPacket.resize(config.fixedPacketSize, 0U);
        }

        const ssize_t sent{
            ::sendto(
                fd,
                requestPacket.data(),
                requestPacket.size(),
                0,
                reinterpret_cast<const struct sockaddr *>(&destination),
                destinationLen)};
        if (sent < 0)
        {
            std::cerr << "[ERROR] vehicle-id sendto failed: " << std::strerror(errno) << "\n";
            ::close(fd);
            return false;
        }

        std::cout << "[INFO] Vehicle-ID request sent to " << config.host << ":" << config.udpPort
                  << " (broadcast=" << (config.udpBroadcast ? "true" : "false") << ")\n";

        const auto deadline{std::chrono::steady_clock::now() + std::chrono::milliseconds(config.timeoutMs)};

        while (true)
        {
            const auto now{std::chrono::steady_clock::now()};
            if (now >= deadline)
            {
                std::cerr << "[ERROR] vehicle-id response timeout\n";
                ::close(fd);
                return false;
            }

            const auto remaining{std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()};
            if (!PollForEvent(fd, POLLIN, static_cast<std::uint32_t>(remaining), error))
            {
                std::cerr << "[ERROR] vehicle-id wait failed: " << error << "\n";
                ::close(fd);
                return false;
            }

            std::vector<std::uint8_t> datagram(2048U, 0U);
            const ssize_t received{
                ::recvfrom(
                    fd,
                    datagram.data(),
                    datagram.size(),
                    0,
                    nullptr,
                    nullptr)};
            if (received < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                std::cerr << "[ERROR] vehicle-id recvfrom failed: " << std::strerror(errno) << "\n";
                ::close(fd);
                return false;
            }

            datagram.resize(static_cast<std::size_t>(received));

            DoipFrame frame;
            if (!ParseDoipPacket(datagram, frame, error))
            {
                continue;
            }

            if (frame.payloadType != kDoipPayloadVehicleIdResponse)
            {
                continue;
            }

            VehicleAnnouncement info;
            if (!ParseVehicleAnnouncement(frame.payload, info, error))
            {
                std::cerr << "[ERROR] vehicle-id parse failed: " << error << "\n";
                ::close(fd);
                return false;
            }

            std::cout << "[OK] Vehicle-ID response received\n";
            std::cout << "     VIN              : " << info.vin << "\n";
            std::cout << "     LogicalAddress   : " << HexU16(info.logicalAddress) << "\n";
            std::cout << "     EID              : " << ToHex(info.eid) << "\n";
            std::cout << "     GID              : " << ToHex(info.gid) << "\n";
            std::cout << "     FurtherAction    : " << HexByte(info.furtherAction) << "\n";
            if (info.hasVinGidStatus)
            {
                std::cout << "     VinGidStatus     : " << HexByte(info.vinGidStatus) << "\n";
            }

            ::close(fd);
            return true;
        }
    }

    bool RunVehicleIdRequestTcp(const Config &config)
    {
        std::string error;
        const int fd{ConnectTcp(config.host, config.tcpPort, config.timeoutMs, error)};
        if (fd < 0)
        {
            std::cerr << "[ERROR] vehicle-id tcp connect failed: " << error << "\n";
            return false;
        }

        const bool sent{
            SendTcpFrame(
                fd,
                config.protocolVersion,
                kDoipPayloadVehicleIdRequest,
                {},
                config.fixedPacketSize,
                config.timeoutMs,
                error)};
        if (!sent)
        {
            std::cerr << "[ERROR] vehicle-id tcp send failed: " << error << "\n";
            ::close(fd);
            return false;
        }

        DoipFrame frame;
        if (!WaitForOneOfPayloadTypes(
                fd,
                {kDoipPayloadVehicleIdResponse, kDoipPayloadGenericNack},
                config.testerAddress,
                config.fixedPacketSize,
                config.timeoutMs,
                frame,
                error))
        {
            std::cerr << "[ERROR] vehicle-id tcp response wait failed: " << error << "\n";
            ::close(fd);
            return false;
        }

        if (frame.payloadType == kDoipPayloadGenericNack)
        {
            std::cerr << "[ERROR] vehicle-id tcp generic NACK: payload=" << ToHex(frame.payload) << "\n";
            ::close(fd);
            return false;
        }

        VehicleAnnouncement info;
        if (!ParseVehicleAnnouncement(frame.payload, info, error))
        {
            std::cerr << "[ERROR] vehicle-id tcp parse failed: " << error << "\n";
            ::close(fd);
            return false;
        }

        std::cout << "[OK] Vehicle-ID response received (tcp)\n";
        std::cout << "     VIN              : " << info.vin << "\n";
        std::cout << "     LogicalAddress   : " << HexU16(info.logicalAddress) << "\n";
        std::cout << "     EID              : " << ToHex(info.eid) << "\n";
        std::cout << "     GID              : " << ToHex(info.gid) << "\n";
        std::cout << "     FurtherAction    : " << HexByte(info.furtherAction) << "\n";
        if (info.hasVinGidStatus)
        {
            std::cout << "     VinGidStatus     : " << HexByte(info.vinGidStatus) << "\n";
        }

        ::close(fd);
        return true;
    }

    bool RunVehicleIdRequest(const Config &config)
    {
        if (config.vehicleIdUseTcp)
        {
            return RunVehicleIdRequestTcp(config);
        }
        return RunVehicleIdRequestUdp(config);
    }

    class TcpDoipSession
    {
    public:
        explicit TcpDoipSession(const Config &config)
            : mProtocolVersion(config.protocolVersion),
              mTesterAddress(config.testerAddress),
              mTargetAddress(config.targetAddress),
              mTimeoutMs(config.timeoutMs),
              mFixedPacketSize(config.fixedPacketSize),
              mAutoTargetFromRouting(config.autoTargetFromRouting)
        {
        }

        ~TcpDoipSession()
        {
            Close();
        }

        bool Connect(const std::string &host, std::uint16_t port)
        {
            std::string error;
            mFd = ConnectTcp(host, port, mTimeoutMs, error);
            if (mFd < 0)
            {
                std::cerr << "[ERROR] TCP connect failed: " << error << "\n";
                return false;
            }

            std::cout << "[INFO] TCP connected to " << host << ":" << port << "\n";
            return true;
        }

        void Close()
        {
            if (mFd >= 0)
            {
                ::close(mFd);
                mFd = -1;
            }
        }

        bool RoutingActivation(bool allowUnsupportedRoutingActivation)
        {
            const std::vector<std::uint8_t> request{
                BuildRoutingActivationRequest(mTesterAddress, mActivationType)};

            std::string error;
            if (!SendTcpFrame(
                    mFd,
                    mProtocolVersion,
                    kDoipPayloadRoutingActivationRequest,
                    request,
                    mFixedPacketSize,
                    mTimeoutMs,
                    error))
            {
                std::cerr << "[ERROR] RoutingActivation send failed: " << error << "\n";
                return false;
            }

            DoipFrame frame;
            if (!WaitForOneOfPayloadTypes(
                    mFd,
                    {kDoipPayloadRoutingActivationResponse, kDoipPayloadGenericNack},
                    mTesterAddress,
                    mFixedPacketSize,
                    mTimeoutMs,
                    frame,
                    error))
            {
                std::cerr << "[ERROR] RoutingActivation response wait failed: " << error << "\n";
                return false;
            }

            if (frame.payloadType == kDoipPayloadGenericNack)
            {
                const std::uint8_t nackCode{
                    frame.payload.empty() ? static_cast<std::uint8_t>(0xFFU) : frame.payload[0U]};
                if (allowUnsupportedRoutingActivation && nackCode == 0x01U)
                {
                    std::cout << "[WARN] RoutingActivation is not supported by target ECU (generic NACK 0x01). "
                              << "Continue with direct diagnostic request.\n";
                    return true;
                }

                std::cerr << "[ERROR] RoutingActivation generic NACK: payload=" << ToHex(frame.payload) << "\n";
                return false;
            }

            RoutingActivationResult result;
            if (!ParseRoutingActivationResponse(frame.payload, result, error))
            {
                std::cerr << "[ERROR] RoutingActivation parse failed: " << error << "\n";
                return false;
            }

            std::cout << "[INFO] RoutingActivation response tester=" << HexU16(result.testerAddress)
                      << " entity=" << HexU16(result.entityAddress)
                      << " code=" << HexByte(result.responseCode)
                      << "(" << RoutingActivationCodeToString(result.responseCode) << ")\n";

            if (mAutoTargetFromRouting)
            {
                mTargetAddress = result.entityAddress;
                std::cout << "[INFO] Diagnostic target-address updated from routing response: "
                          << HexU16(mTargetAddress) << "\n";
            }

            return result.responseCode == kRoutingActivationSuccess ||
                   result.responseCode == kRoutingActivationPending;
        }

        bool SendDiagnosticRequest(const std::vector<std::uint8_t> &uds, std::string &error)
        {
            const auto payload{BuildDiagPayload(mTesterAddress, mTargetAddress, uds)};
            return SendTcpFrame(
                mFd,
                mProtocolVersion,
                kDoipPayloadDiagMessage,
                payload,
                mFixedPacketSize,
                mTimeoutMs,
                error);
        }

        bool ReceiveDiagnosticResponse(DiagResponse &response, std::string &error)
        {
            DoipFrame frame;
            if (!WaitForOneOfPayloadTypes(
                    mFd,
                    {kDoipPayloadDiagAck, kDoipPayloadDiagNack, kDoipPayloadGenericNack},
                    mTesterAddress,
                    mFixedPacketSize,
                    mTimeoutMs,
                    frame,
                    error))
            {
                return false;
            }

            if (frame.payloadType == kDoipPayloadGenericNack)
            {
                error = "received DoIP generic NACK while waiting diagnostic response: " + ToHex(frame.payload);
                return false;
            }

            return ParseDiagPayload(frame, response, error);
        }

        void SetActivationType(std::uint8_t activationType)
        {
            mActivationType = activationType;
        }

    private:
        int mFd{-1};
        std::uint8_t mProtocolVersion{0x02U};
        std::uint16_t mTesterAddress{0x0E80U};
        std::uint16_t mTargetAddress{0x0001U};
        std::uint8_t mActivationType{0x00U};
        std::uint32_t mTimeoutMs{2000U};
        std::size_t mFixedPacketSize{64U};
        bool mAutoTargetFromRouting{true};
    };

    void PrintDiagResponse(const DiagResponse &response)
    {
        std::cout << "[INFO] Diagnostic response type="
                  << (response.isPositiveAck ? "DoIP-ACK" : "DoIP-NACK")
                  << " code=" << HexByte(response.code)
                  << " uds=" << ToHex(response.udsPayload) << "\n";

        if (!response.udsPayload.empty())
        {
            const std::uint8_t sid{response.udsPayload[0U]};
            if (sid == 0x7FU && response.udsPayload.size() >= 3U)
            {
                std::cout << "[INFO] UDS negative response: requestSID="
                          << HexByte(response.udsPayload[1U])
                          << " NRC=" << HexByte(response.udsPayload[2U]) << "\n";
            }
            else if (sid == 0x62U && response.udsPayload.size() >= 3U)
            {
                const std::uint16_t did{
                    static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(response.udsPayload[1U]) << 8U) |
                        response.udsPayload[2U])};
                std::vector<std::uint8_t> data;
                data.assign(response.udsPayload.begin() + 3, response.udsPayload.end());
                std::cout << "[INFO] UDS ReadDataByIdentifier positive response DID="
                          << HexU16(did)
                          << " data=" << ToHex(data) << "\n";
            }
        }
    }

    bool RunRoutingActivationOnly(const Config &config, bool allowUnsupportedRoutingActivation)
    {
        TcpDoipSession session(config);
        session.SetActivationType(config.activationType);

        if (!session.Connect(config.host, config.tcpPort))
        {
            return false;
        }

        return session.RoutingActivation(allowUnsupportedRoutingActivation);
    }

    bool RunOneDiagExchange(const Config &config, const std::vector<std::uint8_t> &uds)
    {
        TcpDoipSession session(config);
        session.SetActivationType(config.activationType);

        if (!session.Connect(config.host, config.tcpPort))
        {
            return false;
        }

        if (!session.RoutingActivation(config.routingActivationOptional))
        {
            return false;
        }

        std::string error;
        if (!session.SendDiagnosticRequest(uds, error))
        {
            std::cerr << "[ERROR] Diagnostic send failed: " << error << "\n";
            return false;
        }

        DiagResponse response;
        if (!session.ReceiveDiagnosticResponse(response, error))
        {
            std::cerr << "[ERROR] Diagnostic response receive failed: " << error << "\n";
            return false;
        }

        PrintDiagResponse(response);
        return true;
    }

    bool RunTxOrRxTest(const Config &config, const std::vector<std::uint8_t> &uds, bool rxFocused)
    {
        TcpDoipSession session(config);
        session.SetActivationType(config.activationType);

        if (!session.Connect(config.host, config.tcpPort))
        {
            return false;
        }

        if (!session.RoutingActivation(config.routingActivationOptional))
        {
            return false;
        }

        std::size_t txSuccess{0U};
        std::size_t rxSuccess{0U};

        for (std::size_t i = 0U; i < config.count; ++i)
        {
            std::string sendError;
            if (!session.SendDiagnosticRequest(uds, sendError))
            {
                std::cerr << "[WARN] request#" << (i + 1U)
                          << " send failed: " << sendError << "\n";
                continue;
            }
            ++txSuccess;

            DiagResponse response;
            std::string receiveError;
            if (!session.ReceiveDiagnosticResponse(response, receiveError))
            {
                std::cerr << "[WARN] request#" << (i + 1U)
                          << " receive failed: " << receiveError << "\n";
            }
            else
            {
                ++rxSuccess;
            }

            if (config.intervalMs > 0U)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.intervalMs));
            }
        }

        std::cout << "[INFO] Test summary total=" << config.count
                  << " txSuccess=" << txSuccess
                  << " rxSuccess=" << rxSuccess
                  << " minRx=" << config.minRx << "\n";

        if (rxFocused)
        {
            const bool success{rxSuccess >= config.minRx};
            if (!success)
            {
                std::cerr << "[ERROR] rx-test failed: rxSuccess < minRx\n";
            }
            return success;
        }

        const bool success{txSuccess == config.count};
        if (!success)
        {
            std::cerr << "[ERROR] tx-test failed: some requests were not transmitted\n";
        }
        return success;
    }

    bool RunFullTest(const Config &config)
    {
        bool overallSuccess{true};

        if (config.requestVehicleIdInFull)
        {
            overallSuccess = RunVehicleIdRequest(config) && overallSuccess;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        overallSuccess = RunOneDiagExchange(config, BuildReadDidRequest(config.did)) && overallSuccess;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        overallSuccess = RunTxOrRxTest(config, BuildReadDidRequest(config.did), false) && overallSuccess;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        overallSuccess = RunTxOrRxTest(config, BuildReadDidRequest(config.did), true) && overallSuccess;

        return overallSuccess;
    }
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg{argv[i]};
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    Config config;
    if (!ParseArgs(argc, argv, config))
    {
        return 1;
    }

    std::vector<std::uint8_t> customUds;
    if ((config.mode == Mode::DiagCustom || config.mode == Mode::TxTest || config.mode == Mode::RxTest) &&
        !config.udsHex.empty())
    {
        if (!ParseUdsHex(config.udsHex, customUds))
        {
            std::cerr << "[ERROR] Invalid --uds format. Use hex bytes like 22F50D or \"22 F5 0D\"\n";
            return 1;
        }
    }

    bool ok{false};
    switch (config.mode)
    {
    case Mode::VehicleId:
        ok = RunVehicleIdRequest(config);
        break;
    case Mode::RoutingActivation:
        ok = RunRoutingActivationOnly(config, false);
        break;
    case Mode::DiagReadDid:
        ok = RunOneDiagExchange(config, BuildReadDidRequest(config.did));
        break;
    case Mode::DiagCustom:
        if (customUds.empty())
        {
            std::cerr << "[ERROR] --mode=diag-custom requires --uds option\n";
            return 1;
        }
        ok = RunOneDiagExchange(config, customUds);
        break;
    case Mode::TxTest:
        if (customUds.empty())
        {
            customUds = BuildReadDidRequest(config.did);
        }
        ok = RunTxOrRxTest(config, customUds, false);
        break;
    case Mode::RxTest:
        if (customUds.empty())
        {
            customUds = BuildReadDidRequest(config.did);
        }
        ok = RunTxOrRxTest(config, customUds, true);
        break;
    case Mode::FullTest:
        ok = RunFullTest(config);
        break;
    }

    if (!ok)
    {
        std::cerr << "[RESULT] FAIL\n";
        return 1;
    }

    std::cout << "[RESULT] PASS\n";
    return 0;
}
