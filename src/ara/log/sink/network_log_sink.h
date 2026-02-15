#ifndef NETWORK_LOG_SINK_H
#define NETWORK_LOG_SINK_H

#include <string>
#include "./log_sink.h"

namespace ara
{
    namespace log
    {
        namespace sink
        {
            /// @brief Log sink implementation that sends logs via UDP.
            /// @note Uses plain-text format for educational purposes
            ///       (not the binary DLT wire protocol).
            class NetworkLogSink : public LogSink
            {
            private:
                static const uint16_t cDefaultPort{3490U};
                static const char *const cDefaultHost;

                std::string mHost;
                uint16_t mPort;
                int mSocketFd;

                void openSocket();
                void closeSocket() noexcept;

            public:
                /// @brief Constructor with default DLT port
                /// @param appId Application ID
                /// @param appDescription Application description
                /// @param host Destination host (default 127.0.0.1)
                /// @param port Destination UDP port (default 3490)
                NetworkLogSink(
                    std::string appId,
                    std::string appDescription,
                    std::string host = "127.0.0.1",
                    uint16_t port = cDefaultPort);

                NetworkLogSink() = delete;
                ~NetworkLogSink() noexcept override;

                void Log(const LogStream &logStream) const override;
            };
        }
    }
}

#endif
