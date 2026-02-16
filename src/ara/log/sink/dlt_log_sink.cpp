/// @file src/ara/log/sink/dlt_log_sink.cpp
/// @brief Implementation for DLT protocol log sink.
/// @details Builds a simplified DLT message (storage header + standard header
///          + extended header + string payload) and sends it over UDP.

#include "./dlt_log_sink.h"

#include <chrono>
#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace ara
{
    namespace log
    {
        namespace sink
        {
            DltLogSink::DltLogSink(
                std::string appId,
                std::string appDescription,
                std::string ecuId,
                std::string host,
                std::uint16_t port)
                : LogSink{appId, appDescription},
                  mEcuId{std::move(ecuId)},
                  mHost{std::move(host)},
                  mPort{port},
                  mSocketFd{-1},
                  mMessageCounter{0U}
            {
                OpenSocket();
            }

            DltLogSink::~DltLogSink() noexcept
            {
                CloseSocket();
            }

            void DltLogSink::OpenSocket()
            {
                mSocketFd = ::socket(AF_INET, SOCK_DGRAM, 0);
                if (mSocketFd < 0)
                {
                    throw std::runtime_error(
                        "Failed to create UDP socket for DLT log sink.");
                }
            }

            void DltLogSink::CloseSocket() noexcept
            {
                if (mSocketFd >= 0)
                {
                    ::close(mSocketFd);
                    mSocketFd = -1;
                }
            }

            void DltLogSink::Write4CharId(
                std::vector<std::uint8_t> &buffer,
                const std::string &id)
            {
                for (std::size_t i = 0U; i < 4U; ++i)
                {
                    buffer.push_back(
                        i < id.size()
                            ? static_cast<std::uint8_t>(id[i])
                            : static_cast<std::uint8_t>(0U));
                }
            }

            void DltLogSink::WriteU16BE(
                std::vector<std::uint8_t> &buffer,
                std::uint16_t value)
            {
                buffer.push_back(
                    static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
                buffer.push_back(
                    static_cast<std::uint8_t>(value & 0xFFU));
            }

            void DltLogSink::WriteU32BE(
                std::vector<std::uint8_t> &buffer,
                std::uint32_t value)
            {
                buffer.push_back(
                    static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
                buffer.push_back(
                    static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
                buffer.push_back(
                    static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
                buffer.push_back(
                    static_cast<std::uint8_t>(value & 0xFFU));
            }

            std::vector<std::uint8_t> DltLogSink::BuildDltMessage(
                const std::string &appId,
                const std::string &contextId,
                const std::string &payload) const
            {
                // DLT message structure (simplified):
                //
                // [Storage Header 16 bytes]
                //   DLT\x01 (4B pattern)
                //   Timestamp seconds (4B LE)
                //   Timestamp microseconds (4B LE)
                //   ECU ID (4B ASCII)
                //
                // [Standard Header 4+ bytes]
                //   HTYP (1B): UEH=1, MSBF=0, WEID=1, WSID=0, WTMS=1, VERS=1
                //   MCNT (1B): message counter
                //   LEN  (2B BE): total message length (after storage header)
                //   ECU ID (4B)
                //   Timestamp (4B BE) — 0.1ms resolution
                //
                // [Extended Header 10 bytes]
                //   MSIN (1B): verbose=1, MSTP=log, MTIN=info
                //   NOAR (1B): number of arguments = 1
                //   APID (4B)
                //   CTID (4B)
                //
                // [Payload]
                //   Type info (4B): string type
                //   String length (2B LE)
                //   String data (NUL terminated)

                std::vector<std::uint8_t> msg;
                msg.reserve(64U + payload.size());

                // --- Storage Header (16 bytes) ---
                msg.push_back('D');
                msg.push_back('L');
                msg.push_back('T');
                msg.push_back(0x01U);

                const auto now{std::chrono::system_clock::now()};
                const auto epoch{now.time_since_epoch()};
                const auto secs{
                    std::chrono::duration_cast<std::chrono::seconds>(epoch)
                        .count()};
                const auto usecs{
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        epoch)
                        .count() %
                    1000000LL};

                // Seconds (4B little-endian).
                const std::uint32_t secU32{
                    static_cast<std::uint32_t>(secs & 0xFFFFFFFFULL)};
                msg.push_back(static_cast<std::uint8_t>(secU32 & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((secU32 >> 8U) & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((secU32 >> 16U) & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((secU32 >> 24U) & 0xFFU));

                // Microseconds (4B little-endian).
                const std::uint32_t usecU32{
                    static_cast<std::uint32_t>(usecs & 0xFFFFFFFFLL)};
                msg.push_back(static_cast<std::uint8_t>(usecU32 & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((usecU32 >> 8U) & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((usecU32 >> 16U) & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((usecU32 >> 24U) & 0xFFU));

                // ECU ID (4B).
                Write4CharId(msg, mEcuId);

                // Record position for length patching.
                const std::size_t stdHeaderStart{msg.size()};

                // --- Standard Header ---
                // HTYP: UEH=1 (bit0), MSBF=0 (bit1), WEID=1 (bit2),
                //        WSID=0 (bit3), WTMS=1 (bit4), Version=1 (bits5-7 = 001)
                const std::uint8_t htyp{
                    0x01U | 0x04U | 0x10U | (0x01U << 5U)};
                msg.push_back(htyp);

                // MCNT.
                const std::uint32_t counter{
                    mMessageCounter.fetch_add(
                        1U, std::memory_order_relaxed)};
                msg.push_back(
                    static_cast<std::uint8_t>(counter & 0xFFU));

                // LEN placeholder (2B BE) — will be patched.
                const std::size_t lenPos{msg.size()};
                msg.push_back(0U);
                msg.push_back(0U);

                // ECU ID (4B).
                Write4CharId(msg, mEcuId);

                // Timestamp (4B BE, 0.1ms resolution).
                const std::uint64_t tsMs{
                    static_cast<std::uint64_t>(secs) * 10000ULL +
                    static_cast<std::uint64_t>(usecs) / 100ULL};
                WriteU32BE(msg, static_cast<std::uint32_t>(
                                    tsMs & 0xFFFFFFFFULL));

                // --- Extended Header (10 bytes) ---
                // MSIN: verbose=1 (bit0), MSTP=Log=0x0 (bits1-3),
                //        MTIN=Info=0x4 (bits4-7)
                const std::uint8_t msin{0x01U | (0x04U << 4U)};
                msg.push_back(msin);

                // NOAR: 1 argument (the string payload).
                msg.push_back(0x01U);

                // APID.
                Write4CharId(msg, appId);

                // CTID.
                Write4CharId(msg, contextId);

                // --- Payload ---
                // Type info for string (4B LE): 0x00000200 (DLT_TYPE_INFO_STRG)
                msg.push_back(0x00U);
                msg.push_back(0x02U);
                msg.push_back(0x00U);
                msg.push_back(0x00U);

                // String length including NUL (2B LE).
                const std::uint16_t strLen{
                    static_cast<std::uint16_t>(payload.size() + 1U)};
                msg.push_back(
                    static_cast<std::uint8_t>(strLen & 0xFFU));
                msg.push_back(
                    static_cast<std::uint8_t>((strLen >> 8U) & 0xFFU));

                // String data.
                for (const char ch : payload)
                {
                    msg.push_back(static_cast<std::uint8_t>(ch));
                }
                msg.push_back(0U); // NUL terminator.

                // Patch LEN field (total from standard header to end).
                const std::uint16_t totalLen{
                    static_cast<std::uint16_t>(msg.size() - stdHeaderStart)};
                msg[lenPos] = static_cast<std::uint8_t>(
                    (totalLen >> 8U) & 0xFFU);
                msg[lenPos + 1U] = static_cast<std::uint8_t>(
                    totalLen & 0xFFU);

                return msg;
            }

            void DltLogSink::Log(const LogStream &logStream) const
            {
                if (mSocketFd < 0)
                {
                    return;
                }

                LogStream timestamp{GetTimestamp()};
                LogStream appstamp{GetAppstamp()};
                timestamp << cWhitespace << appstamp
                          << cWhitespace << logStream;
                const std::string payload{timestamp.ToString()};

                // Extract app ID from appstamp (first 4 chars).
                const std::string appStr{appstamp.ToString()};
                std::string shortAppId{appStr.substr(
                    0U, std::min<std::size_t>(appStr.size(), 4U))};
                const std::string ctxId{"DFLT"};

                const auto dltMsg{
                    BuildDltMessage(shortAppId, ctxId, payload)};

                struct sockaddr_in destAddr {};
                destAddr.sin_family = AF_INET;
                destAddr.sin_port = htons(mPort);
                ::inet_pton(AF_INET, mHost.c_str(), &destAddr.sin_addr);

                ::sendto(
                    mSocketFd,
                    dltMsg.data(),
                    dltMsg.size(),
                    0,
                    reinterpret_cast<const struct sockaddr *>(&destAddr),
                    sizeof(destAddr));
            }
        }
    }
}
