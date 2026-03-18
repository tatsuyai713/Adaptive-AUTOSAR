/// @file src/ara/sm/network_interface_controller.cpp
/// @brief Implementation of NetworkInterfaceController.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./network_interface_controller.h"

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
