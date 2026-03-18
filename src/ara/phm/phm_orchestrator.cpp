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
            UpdatePlatformState();
            return {};
        }

        core::Result<void> PhmOrchestrator::ReportSupervisionRecovery(
            const std::string &entityName)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mEntities.find(entityName);
            if (it == mEntities.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kNotFound));
            }
            it->second.Status = GlobalSupervisionStatus::kOk;
            UpdatePlatformState();
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
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &kv : mEntities)
            {
                kv.second.FailureCount = 0;
                kv.second.Status = GlobalSupervisionStatus::kDeactivated;
            }
            auto oldState = mCurrentState;
            mCurrentState = PlatformHealthState::kNormal;
            if (mCallback && oldState != mCurrentState)
            {
                mCallback(oldState, mCurrentState);
            }
        }

        size_t PhmOrchestrator::GetEntityCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEntities.size();
        }

        void PhmOrchestrator::UpdatePlatformState()
        {
            if (mEntities.empty()) return;

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

            PlatformHealthState newState = PlatformHealthState::kNormal;
            if (ratio >= mCriticalThreshold)
                newState = PlatformHealthState::kCritical;
            else if (ratio > mDegradedThreshold)
                newState = PlatformHealthState::kDegraded;

            if (newState != mCurrentState)
            {
                auto oldState = mCurrentState;
                mCurrentState = newState;
                if (mCallback)
                {
                    mCallback(oldState, newState);
                }
            }
        }
    }
}
