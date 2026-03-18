/// @file src/ara/com/secoc/freshness_sync_manager.cpp
/// @brief Cross-ECU freshness sync manager implementation.

#include "./freshness_sync_manager.h"
#include <algorithm>
#include <cstring>

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            FreshnessSyncManager::FreshnessSyncManager(std::string localEcuId)
                : mLocalEcuId{std::move(localEcuId)}
            {
            }

            core::Result<void> FreshnessSyncManager::RegisterPdu(
                PduId pdu, ReplayWindowConfig window)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mEntries.count(pdu) > 0)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kConfigurationError));
                }
                FreshnessSyncEntry entry;
                entry.Window = window;
                entry.LastSyncTime = std::chrono::steady_clock::now();
                mEntries[pdu] = entry;
                return core::Result<void>::FromValue();
            }

            core::Result<uint64_t> FreshnessSyncManager::GetLocalCounter(
                PduId pdu) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return core::Result<uint64_t>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kFreshnessCounterFailed));
                }
                return core::Result<uint64_t>::FromValue(it->second.LocalCounter);
            }

            core::Result<uint64_t> FreshnessSyncManager::IncrementCounter(
                PduId pdu)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return core::Result<uint64_t>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kFreshnessCounterFailed));
                }
                ++(it->second.LocalCounter);
                return core::Result<uint64_t>::FromValue(it->second.LocalCounter);
            }

            FreshnessVerifyResult FreshnessSyncManager::VerifyFreshness(
                PduId pdu, uint64_t receivedCounter) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return FreshnessVerifyResult::kRejectedUnknownPdu;
                }

                const auto &entry = it->second;
                uint64_t localVal = entry.RemoteCounter;

                // Check age
                auto elapsed = std::chrono::steady_clock::now() - entry.LastSyncTime;
                auto elapsedMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(elapsed).count();
                if (entry.Window.MaxAgeMs > 0 &&
                    static_cast<uint64_t>(elapsedMs) > entry.Window.MaxAgeMs)
                {
                    return FreshnessVerifyResult::kRejectedExpired;
                }

                // Check backward tolerance
                if (receivedCounter < localVal)
                {
                    uint64_t diff = localVal - receivedCounter;
                    if (diff > entry.Window.BackwardTolerance)
                    {
                        return FreshnessVerifyResult::kRejectedTooOld;
                    }
                }

                // Check forward tolerance
                if (receivedCounter > localVal)
                {
                    uint64_t diff = receivedCounter - localVal;
                    if (diff > entry.Window.ForwardTolerance)
                    {
                        return FreshnessVerifyResult::kRejectedTooNew;
                    }
                }

                return FreshnessVerifyResult::kAccepted;
            }

            core::Result<void> FreshnessSyncManager::ProcessSyncResponse(
                const FreshnessSyncResponse &response)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(response.Pdu);
                if (it == mEntries.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kFreshnessCounterFailed));
                }

                it->second.RemoteCounter = response.AcknowledgedCounter;
                it->second.State = response.SyncState;
                it->second.LastSyncTime = std::chrono::steady_clock::now();
                return core::Result<void>::FromValue();
            }

            core::Result<FreshnessSyncRequest>
            FreshnessSyncManager::CreateSyncRequest(PduId pdu) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return core::Result<FreshnessSyncRequest>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kFreshnessCounterFailed));
                }

                FreshnessSyncRequest req;
                req.Pdu = pdu;
                req.CounterValue = it->second.LocalCounter;
                req.TimestampMs = NowMs();
                req.SourceEcuId = mLocalEcuId;
                return core::Result<FreshnessSyncRequest>::FromValue(req);
            }

            void FreshnessSyncManager::SetSyncMessageSender(
                SyncMessageSender sender)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mSender = std::move(sender);
            }

            FreshnessSyncState FreshnessSyncManager::GetSyncState(
                PduId pdu) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return FreshnessSyncState::kUnsynchronized;
                }
                return it->second.State;
            }

            const std::string &FreshnessSyncManager::GetLocalEcuId() const noexcept
            {
                return mLocalEcuId;
            }

            std::vector<uint8_t> FreshnessSyncManager::PersistState() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                std::vector<uint8_t> result;

                uint32_t count = static_cast<uint32_t>(mEntries.size());
                result.resize(4);
                std::memcpy(result.data(), &count, 4);

                for (const auto &kv : mEntries)
                {
                    // PduId (2 bytes) + LocalCounter (8 bytes)
                    size_t pos = result.size();
                    result.resize(pos + 10);
                    uint16_t pdu = kv.first;
                    uint64_t counter = kv.second.LocalCounter;
                    std::memcpy(result.data() + pos, &pdu, 2);
                    std::memcpy(result.data() + pos + 2, &counter, 8);
                }

                return result;
            }

            core::Result<void> FreshnessSyncManager::RestoreState(
                const std::vector<uint8_t> &data)
            {
                std::lock_guard<std::mutex> lock(mMutex);

                if (data.size() < 4)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kInvalidPayloadLength));
                }

                uint32_t count = 0;
                std::memcpy(&count, data.data(), 4);

                if (data.size() < 4 + count * 10)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(
                            SecOcErrc::kInvalidPayloadLength));
                }

                size_t offset = 4;
                for (uint32_t i = 0; i < count; ++i)
                {
                    uint16_t pdu = 0;
                    uint64_t counter = 0;
                    std::memcpy(&pdu, data.data() + offset, 2);
                    std::memcpy(&counter, data.data() + offset + 2, 8);
                    offset += 10;

                    auto it = mEntries.find(pdu);
                    if (it != mEntries.end())
                    {
                        it->second.LocalCounter = counter;
                    }
                }

                return core::Result<void>::FromValue();
            }

            size_t FreshnessSyncManager::RegisteredPduCount() const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mEntries.size();
            }

            uint64_t FreshnessSyncManager::NowMs() const
            {
                auto now = std::chrono::steady_clock::now();
                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch())
                        .count());
            }
        }
    }
}
