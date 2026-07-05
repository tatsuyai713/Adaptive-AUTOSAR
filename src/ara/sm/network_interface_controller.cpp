/// @file src/ara/sm/network_interface_controller.cpp
/// @brief Implementation of NetworkInterfaceController.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./network_interface_controller.h"
#if defined(__linux__) || defined(__QNX__)
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

namespace ara
{
    namespace sm
    {
        namespace
        {
            core::Result<void> InvalidArgument()
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            core::Result<void> NetworkUnavailable()
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }

            bool IsValidInterfaceName(const std::string &name)
            {
                if (name.empty())
                {
                    return false;
                }
#if defined(__linux__) || defined(__QNX__)
                return name.size() < IFNAMSIZ;
#else
                return true;
#endif
            }

            bool IsValidIpv4Address(const std::string &address)
            {
                std::size_t start = 0;
                int partCount = 0;
                while (start < address.size())
                {
                    const std::size_t end = address.find('.', start);
                    const std::size_t length =
                        (end == std::string::npos ? address.size() : end) - start;
                    if (length == 0 || length > 3)
                    {
                        return false;
                    }

                    int value = 0;
                    for (std::size_t i = start; i < start + length; ++i)
                    {
                        const char ch = address[i];
                        if (ch < '0' || ch > '9')
                        {
                            return false;
                        }
                        value = (value * 10) + (ch - '0');
                    }
                    if (value > 255)
                    {
                        return false;
                    }

                    ++partCount;
                    if (end == std::string::npos)
                    {
                        break;
                    }
                    start = end + 1;
                }
                return partCount == 4;
            }
        }

        core::Result<void> NetworkInterfaceController::RegisterInterface(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!IsValidInterfaceName(name))
            {
                return InvalidArgument();
            }
            if (mInterfaces.count(name))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kAlreadyInState));
            }
            InterfaceInfo info;
            info.Name = name;
            mInterfaces[name] = info;
            return {};
        }

        core::Result<void> NetworkInterfaceController::EnableInterface(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return NetworkUnavailable();
            }
#if defined(__linux__) || defined(__QNX__)
            // Bring up the OS network interface via SIOCSIFFLAGS.
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0)
            {
                return NetworkUnavailable();
            }
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                if (::ioctl(sockfd, SIOCGIFFLAGS, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
                if (::ioctl(sockfd, SIOCSIFFLAGS, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ::close(sockfd);
            }
#endif
            it->second.AdminEnabled = true;
            it->second.Status = LinkStatus::kUp;
            return {};
        }

        core::Result<void> NetworkInterfaceController::DisableInterface(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return NetworkUnavailable();
            }
#if defined(__linux__) || defined(__QNX__)
            // Bring down the OS network interface via SIOCSIFFLAGS.
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0)
            {
                return NetworkUnavailable();
            }
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                if (::ioctl(sockfd, SIOCGIFFLAGS, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
                if (::ioctl(sockfd, SIOCSIFFLAGS, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ::close(sockfd);
            }
#endif
            it->second.AdminEnabled = false;
            it->second.Status = LinkStatus::kDown;
            return {};
        }

        core::Result<void> NetworkInterfaceController::SetMtu(
            const std::string &name, uint32_t mtu)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return NetworkUnavailable();
            }
            if (mtu == 0 || mtu > 65535)
            {
                return InvalidArgument();
            }
#if defined(__linux__) || defined(__QNX__)
            // Set MTU via SIOCSIFMTU ioctl (requires CAP_NET_ADMIN).
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0)
            {
                return NetworkUnavailable();
            }
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                ifr.ifr_mtu = static_cast<int>(mtu);
                if (::ioctl(sockfd, SIOCSIFMTU, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ::close(sockfd);
            }
#endif
            it->second.Mtu = mtu;
            return {};
        }

        core::Result<void> NetworkInterfaceController::SetIpv4Address(
            const std::string &name, const std::string &address)
        {
            if (!IsValidIpv4Address(address))
            {
                return InvalidArgument();
            }
#if defined(__linux__) || defined(__QNX__)
            struct in_addr parsedAddress{};
            if (::inet_pton(AF_INET, address.c_str(), &parsedAddress) != 1)
            {
                return InvalidArgument();
            }
#endif
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return NetworkUnavailable();
            }
#if defined(__linux__) || defined(__QNX__)
            // Set IPv4 address via SIOCSIFADDR ioctl (requires CAP_NET_ADMIN).
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0)
            {
                return NetworkUnavailable();
            }
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                auto *saddr =
                    reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr);
                saddr->sin_family = AF_INET;
                saddr->sin_addr = parsedAddress;
                if (::ioctl(sockfd, SIOCSIFADDR, &ifr) != 0)
                {
                    ::close(sockfd);
                    return NetworkUnavailable();
                }
                ::close(sockfd);
            }
#endif
            it->second.Ipv4Address = address;
            return {};
        }

        core::Result<InterfaceInfo>
        NetworkInterfaceController::GetInterfaceInfo(
            const std::string &name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return core::Result<InterfaceInfo>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
            return it->second;
        }

        std::vector<InterfaceInfo>
        NetworkInterfaceController::GetAllInterfaces() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<InterfaceInfo> result;
            result.reserve(mInterfaces.size());
            for (const auto &kv : mInterfaces)
            {
                result.push_back(kv.second);
            }
            return result;
        }

        core::Result<bool> NetworkInterfaceController::IsEnabled(
            const std::string &name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
            return it->second.AdminEnabled;
        }
    }
}
