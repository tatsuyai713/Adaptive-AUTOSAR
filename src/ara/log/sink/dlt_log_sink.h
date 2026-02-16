/// @file src/ara/log/sink/dlt_log_sink.h
/// @brief DLT (Diagnostic Log and Trace) protocol log sink.
/// @details Sends log messages in AUTOSAR DLT-compatible binary format
///          over UDP for interoperability with DLT viewers (e.g. dlt-viewer).

#ifndef DLT_LOG_SINK_H
#define DLT_LOG_SINK_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            class DltLogSink : public LogSink
            {
            public:
                DltLogSink(
                    std::string appId,
                    std::string appDescription,
                    std::string ecuId = "ECU1",
                    std::string host = "127.0.0.1",
                    std::uint16_t port = 3490U);

                DltLogSink() = delete;
                ~DltLogSink() noexcept override;

                void Log(const LogStream &logStream) const override;

            private:
                std::string mEcuId;
                std::string mHost;
                std::uint16_t mPort;
                int mSocketFd;
                mutable std::atomic<std::uint32_t> mMessageCounter;

                void OpenSocket();
                void CloseSocket() noexcept;

                std::vector<std::uint8_t> BuildDltMessage(
                    const std::string &appId,
                    const std::string &contextId,
                    const std::string &payload) const;

                static void Write4CharId(
                    std::vector<std::uint8_t> &buffer,
                    const std::string &id);

                static void WriteU16BE(
                    std::vector<std::uint8_t> &buffer,
                    std::uint16_t value);

                static void WriteU32BE(
                    std::vector<std::uint8_t> &buffer,
                    std::uint32_t value);
            };
        }
    }
}

#endif
