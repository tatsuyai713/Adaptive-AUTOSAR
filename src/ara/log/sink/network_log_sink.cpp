#include "./network_log_sink.h"

#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace ara
{
    namespace log
    {
        namespace sink
        {
            const char *const NetworkLogSink::cDefaultHost{"127.0.0.1"};

            NetworkLogSink::NetworkLogSink(
                std::string appId,
                std::string appDescription,
                std::string host,
                uint16_t port)
                : LogSink{appId, appDescription},
                  mHost{std::move(host)},
                  mPort{port},
                  mSocketFd{-1}
            {
                openSocket();
            }

            NetworkLogSink::~NetworkLogSink() noexcept
            {
                closeSocket();
            }

            void NetworkLogSink::openSocket()
            {
                mSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
                if (mSocketFd < 0)
                {
                    throw std::runtime_error(
                        "Failed to create UDP socket for network log sink.");
                }
            }

            void NetworkLogSink::closeSocket() noexcept
            {
                if (mSocketFd >= 0)
                {
                    close(mSocketFd);
                    mSocketFd = -1;
                }
            }

            void NetworkLogSink::Log(const LogStream &logStream) const
            {
                if (mSocketFd < 0)
                {
                    return;
                }

                LogStream _timestamp = GetTimestamp();
                LogStream _appstamp = GetAppstamp();
                _timestamp << cWhitespace << _appstamp << cWhitespace << logStream;
                std::string _message = _timestamp.ToString();

                struct sockaddr_in _destAddr;
                std::memset(&_destAddr, 0, sizeof(_destAddr));
                _destAddr.sin_family = AF_INET;
                _destAddr.sin_port = htons(mPort);
                inet_pton(AF_INET, mHost.c_str(), &_destAddr.sin_addr);

                sendto(
                    mSocketFd,
                    _message.c_str(),
                    _message.size(),
                    0,
                    reinterpret_cast<const struct sockaddr *>(&_destAddr),
                    sizeof(_destAddr));
            }
        }
    }
}
