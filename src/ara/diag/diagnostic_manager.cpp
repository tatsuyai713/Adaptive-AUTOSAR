/// @file src/ara/diag/diagnostic_manager.cpp
/// @brief Implementation of DiagnosticManager.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./diagnostic_manager.h"
#include <algorithm>

namespace ara
{
    namespace diag
    {
        DiagnosticManager::DiagnosticManager()
            : mTiming{},
              mNextRequestId{1},
              mTotalRequests{0}
        {
        }

        void DiagnosticManager::SetResponseTiming(const ResponseTiming &timing)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mTiming = timing;
        }

        ResponseTiming DiagnosticManager::GetResponseTiming() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mTiming;
        }

        core::Result<void> DiagnosticManager::RegisterService(
            uint8_t serviceId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mServices.count(serviceId))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kAlreadyOffered));
            }
            ServiceStats stats;
            stats.ServiceId = serviceId;
            mServices[serviceId] = stats;
            return {};
        }

        core::Result<void> DiagnosticManager::UnregisterService(
            uint8_t serviceId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mServices.find(serviceId);
            if (it == mServices.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kNotOffered));
            }
            mServices.erase(it);
            return {};
        }

        core::Result<uint32_t> DiagnosticManager::SubmitRequest(
            uint8_t serviceId,
            uint16_t subFunction,
            const std::vector<uint8_t> &payload)
        {
            (void)payload; // Payload stored by routing layer.

            std::lock_guard<std::mutex> lock(mMutex);
            auto svcIt = mServices.find(serviceId);
            if (svcIt == mServices.end())
            {
                return core::Result<uint32_t>::FromError(
                    MakeErrorCode(DiagErrc::kNotOffered));
            }

            uint32_t reqId = mNextRequestId++;
            PendingRequest req;
            req.RequestId = reqId;
            req.ServiceId = serviceId;
            req.SubFunction = subFunction;
            req.ArrivalTime = std::chrono::steady_clock::now();
            req.ResponsePending = false;
            mPending[reqId] = req;

            svcIt->second.RequestCount++;
            svcIt->second.LastRequestTime = req.ArrivalTime;
            ++mTotalRequests;

            return reqId;
        }

        core::Result<void> DiagnosticManager::CompleteRequest(
            uint32_t requestId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mPending.find(requestId);
            if (it == mPending.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            auto svcIt = mServices.find(it->second.ServiceId);
            if (svcIt != mServices.end())
            {
                svcIt->second.SuccessCount++;
            }
            mPending.erase(it);
            return {};
        }

        core::Result<void> DiagnosticManager::RejectRequest(
            uint32_t requestId, uint8_t nrc)
        {
            (void)nrc;
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mPending.find(requestId);
            if (it == mPending.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            auto svcIt = mServices.find(it->second.ServiceId);
            if (svcIt != mServices.end())
            {
                svcIt->second.NrcCount++;
            }
            mPending.erase(it);
            return {};
        }

        uint32_t DiagnosticManager::CheckTimingConstraints()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto now = std::chrono::steady_clock::now();
            uint32_t rpCount = 0;

            for (auto &kv : mPending)
            {
                auto &req = kv.second;
                if (req.ResponsePending) continue;

                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - req.ArrivalTime);

                uint16_t threshold = mTiming.P2ServerMs;
                if (threshold > 0 && elapsed.count() >= threshold)
                {
                    req.ResponsePending = true;
                    ++rpCount;
                    if (mRpCallback)
                    {
                        mRpCallback(req.RequestId, req.ServiceId);
                    }
                }
            }
            return rpCount;
        }

        void DiagnosticManager::SetResponsePendingCallback(
            ResponsePendingCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRpCallback = std::move(cb);
        }

        core::Result<ServiceStats> DiagnosticManager::GetServiceStats(
            uint8_t serviceId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mServices.find(serviceId);
            if (it == mServices.end())
            {
                return core::Result<ServiceStats>::FromError(
                    MakeErrorCode(DiagErrc::kNotOffered));
            }
            return it->second;
        }

        std::vector<PendingRequest> DiagnosticManager::GetPendingRequests()
            const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<PendingRequest> result;
            result.reserve(mPending.size());
            for (const auto &kv : mPending)
            {
                result.push_back(kv.second);
            }
            return result;
        }

        uint32_t DiagnosticManager::GetTotalRequestCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mTotalRequests;
        }
    }
}
