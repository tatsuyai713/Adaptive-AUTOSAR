/// @file src/ara/iam/capability_manager.cpp
/// @brief Implementation of CapabilityManager (SWS_IAM_00200).

#include "./capability_manager.h"

namespace ara
{
    namespace iam
    {
        core::Result<void> CapabilityManager::Grant(
            const std::string &subjectId,
            const std::string &capability)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCapabilities[subjectId].insert(capability);
            return core::Result<void>{};
        }

        core::Result<void> CapabilityManager::Revoke(
            const std::string &subjectId,
            const std::string &capability)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCapabilities.find(subjectId);
            if (it == mCapabilities.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kGrantNotFound));
            }
            it->second.erase(capability);
            return core::Result<void>{};
        }

        bool CapabilityManager::HasCapability(
            const std::string &subjectId,
            const std::string &capability) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCapabilities.find(subjectId);
            if (it == mCapabilities.end())
            {
                return false;
            }
            return it->second.count(capability) > 0;
        }

        std::set<std::string> CapabilityManager::GetCapabilities(
            const std::string &subjectId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mCapabilities.find(subjectId);
            if (it == mCapabilities.end())
            {
                return {};
            }
            return it->second;
        }

        void CapabilityManager::ClearCapabilities(const std::string &subjectId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCapabilities.erase(subjectId);
        }
    }
}
