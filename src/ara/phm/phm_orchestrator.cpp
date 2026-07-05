/// @file src/ara/phm/phm_orchestrator.cpp
/// @brief Implementation of PhmOrchestrator.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./phm_orchestrator.h"
#include "./phm_error_domain.h"

namespace ara
{
    namespace phm
    {
        PhmOrchestrator::PhmOrchestrator(
            double degradedThreshold,
            double criticalThreshold)
            : mDegradedThreshold{degradedThreshold},
              mCriticalThreshold{criticalThreshold},
              mCurrentState{PlatformHealthState::kNormal}
        {
        }

        core::Result<void> PhmOrchestrator::RegisterEntity(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mEntities.count(name))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kAlreadyExists));
            }
            EntityHealthSnapshot snap;
            snap.EntityName = name;
            mEntities[name] = snap;
            return {};
        }

        core::Result<void> PhmOrchestrator::UnregisterEntity(
            const std::string &name)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntities.find(name);
            if (it == mEntities.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kNotFound));
            }
            mEntities.erase(it);
            return {};
        }

        core::Result<void> PhmOrchestrator::ReportSupervisionFailure(
            const std::string &entityName,
            TypeOfSupervision type)
        {
            PlatformHealthCallback callback;
            PlatformHealthState oldState{PlatformHealthState::kNormal};
            PlatformHealthState newState{PlatformHealthState::kNormal};
            bool changed{false};
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntities.find(entityName);
                if (it == mEntities.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PhmErrc::kNotFound));
                }
                it->second.Status = GlobalSupervisionStatus::kFailed;
                it->second.LastFailedSupervision = type;
                it->second.FailureCount++;
                it->second.LastFailureTime = std::chrono::steady_clock::now();
                changed = UpdatePlatformState(oldState, newState);
                callback = mCallback;
            }

            if (changed && callback)
            {
                callback(oldState, newState);
            }
            return {};
        }

        core::Result<void> PhmOrchestrator::ReportSupervisionRecovery(
            const std::string &entityName)
        {
            PlatformHealthCallback callback;
            PlatformHealthState oldState{PlatformHealthState::kNormal};
            PlatformHealthState newState{PlatformHealthState::kNormal};
            bool changed{false};
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntities.find(entityName);
                if (it == mEntities.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(PhmErrc::kNotFound));
                }
                it->second.Status = GlobalSupervisionStatus::kOk;
                changed = UpdatePlatformState(oldState, newState);
                callback = mCallback;
            }

            if (changed && callback)
            {
                callback(oldState, newState);
            }
            return {};
        }

        PlatformHealthState PhmOrchestrator::EvaluatePlatformHealth() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCurrentState;
        }

        core::Result<EntityHealthSnapshot> PhmOrchestrator::GetEntityHealth(
            const std::string &entityName) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntities.find(entityName);
            if (it == mEntities.end())
            {
                return core::Result<EntityHealthSnapshot>::FromError(
                    MakeErrorCode(PhmErrc::kNotFound));
            }
            return it->second;
        }

        std::vector<EntityHealthSnapshot>
        PhmOrchestrator::GetAllEntityHealth() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<EntityHealthSnapshot> result;
            result.reserve(mEntities.size());
            for (const auto &kv : mEntities)
            {
                result.push_back(kv.second);
            }
            return result;
        }

        void PhmOrchestrator::SetPlatformHealthCallback(
            PlatformHealthCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(cb);
        }

        void PhmOrchestrator::ResetAllCounters()
        {
            PlatformHealthCallback callback;
            PlatformHealthState oldState{PlatformHealthState::kNormal};
            PlatformHealthState newState{PlatformHealthState::kNormal};
            bool changed{false};
            {
                std::lock_guard<std::mutex> lock(mMutex);
                for (auto &kv : mEntities)
                {
                    kv.second.FailureCount = 0;
                    kv.second.Status = GlobalSupervisionStatus::kDeactivated;
                }
                oldState = mCurrentState;
                mCurrentState = PlatformHealthState::kNormal;
                newState = mCurrentState;
                changed = oldState != newState;
                callback = mCallback;
            }

            if (changed && callback)
            {
                callback(oldState, newState);
            }
        }

        size_t PhmOrchestrator::GetEntityCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEntities.size();
        }

        bool PhmOrchestrator::UpdatePlatformState(
            PlatformHealthState &oldState,
            PlatformHealthState &newState)
        {
            if (mEntities.empty()) return false;

            uint32_t failedCount = 0;
            for (const auto &kv : mEntities)
            {
                if (kv.second.Status == GlobalSupervisionStatus::kFailed ||
                    kv.second.Status == GlobalSupervisionStatus::kExpired)
                {
                    ++failedCount;
                }
            }

            double ratio = static_cast<double>(failedCount) /
                           static_cast<double>(mEntities.size());

            PlatformHealthState computedState = PlatformHealthState::kNormal;
            if (ratio >= mCriticalThreshold)
                computedState = PlatformHealthState::kCritical;
            else if (ratio > mDegradedThreshold)
                computedState = PlatformHealthState::kDegraded;

            if (computedState != mCurrentState)
            {
                oldState = mCurrentState;
                newState = computedState;
                mCurrentState = computedState;
                return true;
            }
            return false;
        }
    }
}
