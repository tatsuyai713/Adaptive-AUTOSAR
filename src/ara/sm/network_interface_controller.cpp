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
        core::Result<void> NetworkInterfaceController::RegisterInterface(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (name.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
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
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
#if defined(__linux__) || defined(__QNX__)
            // Bring up the OS network interface via SIOCSIFFLAGS.
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd >= 0)
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                if (::ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0)
                {
                    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
                    ::ioctl(sockfd, SIOCSIFFLAGS, &ifr);
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
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
#if defined(__linux__) || defined(__QNX__)
            // Bring down the OS network interface via SIOCSIFFLAGS.
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd >= 0)
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                if (::ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0)
                {
                    ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
                    ::ioctl(sockfd, SIOCSIFFLAGS, &ifr);
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
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
            if (mtu == 0 || mtu > 65535)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }
#if defined(__linux__) || defined(__QNX__)
            // Set MTU via SIOCSIFMTU ioctl (requires CAP_NET_ADMIN).
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd >= 0)
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                ifr.ifr_mtu = static_cast<int>(mtu);
                ::ioctl(sockfd, SIOCSIFMTU, &ifr);
                ::close(sockfd);
            }
#endif
            it->second.Mtu = mtu;
            return {};
        }

        core::Result<void> NetworkInterfaceController::SetIpv4Address(
            const std::string &name, const std::string &address)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mInterfaces.find(name);
            if (it == mInterfaces.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kNetworkUnavailable));
            }
#if defined(__linux__) || defined(__QNX__)
            // Set IPv4 address via SIOCSIFADDR ioctl (requires CAP_NET_ADMIN).
            int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd >= 0)
            {
                struct ifreq ifr{};
                std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
                auto *saddr =
                    reinterpret_cast<struct sockaddr_in *>(&ifr.ifr_addr);
                saddr->sin_family = AF_INET;
                if (::inet_pton(AF_INET, address.c_str(),
                                &saddr->sin_addr) == 1)
                {
                    ::ioctl(sockfd, SIOCSIFADDR, &ifr);
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
