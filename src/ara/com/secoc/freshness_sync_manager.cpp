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
                PduId pdu, uint64_t receivedCounter)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pdu);
                if (it == mEntries.end())
                {
                    return FreshnessVerifyResult::kRejectedUnknownPdu;
                }

                auto &entry = it->second;
                uint64_t localVal = entry.RemoteCounter;

                // Check age since last sync
                auto elapsed = std::chrono::steady_clock::now() - entry.LastSyncTime;
                auto elapsedMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(elapsed).count();
                if (entry.Window.MaxAgeMs > 0 &&
                    static_cast<uint64_t>(elapsedMs) > entry.Window.MaxAgeMs)
                {
                    return FreshnessVerifyResult::kRejectedExpired;
                }

                // Replay / backward tolerance check.
                // The initial RemoteCounter=0 is a sentinel (no counter accepted yet);
                // replay protection only activates once HasAny is true to avoid
                // incorrectly rejecting a legitimate first counter of value 0.
                if (entry.HasAny && receivedCounter <= localVal)
                {
                    uint64_t diff = localVal - receivedCounter;
                    // diff==0 is an exact replay; diff>BackwardTolerance is too far back.
                    if (diff == 0 || diff > entry.Window.BackwardTolerance)
                    {
                        return FreshnessVerifyResult::kRejectedTooOld;
                    }
                }

                // Check forward tolerance
                if (receivedCounter > localVal + entry.Window.ForwardTolerance)
                {
                    return FreshnessVerifyResult::kRejectedTooNew;
                }

                // Advance the window to prevent replay: RemoteCounter tracks the
                // highest accepted counter from the remote ECU (SecOC monotonic window).
                entry.HasAny = true;
                if (receivedCounter > entry.RemoteCounter)
                {
                    entry.RemoteCounter = receivedCounter;
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

            core::Result<FreshnessSyncRequest>
            FreshnessSyncManager::SendSyncRequest(PduId pdu)
            {
                SyncMessageSender sender;
                FreshnessSyncRequest req;

                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    auto it = mEntries.find(pdu);
                    if (it == mEntries.end())
                    {
                        return core::Result<FreshnessSyncRequest>::FromError(
                            MakeErrorCode(SecOcErrc::kFreshnessCounterFailed));
                    }
                    if (!mSender)
                    {
                        return core::Result<FreshnessSyncRequest>::FromError(
                            MakeErrorCode(SecOcErrc::kConfigurationError));
                    }
                    req.Pdu = pdu;
                    req.CounterValue = it->second.LocalCounter;
                    req.TimestampMs = NowMs();
                    req.SourceEcuId = mLocalEcuId;
                    sender = mSender;
                }

                // Invoke the sender outside the lock to avoid potential deadlocks
                // if the callback re-enters the manager.
                sender(req);
                return core::Result<FreshnessSyncRequest>::FromValue(req);
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
                static constexpr size_t cEntryBytes = 19u;
                result.reserve(4u + static_cast<size_t>(count) * cEntryBytes);
                result.resize(4);
                std::memcpy(result.data(), &count, 4);

                for (const auto &kv : mEntries)
                {
                    // Per-entry layout (19 bytes):
                    //   2B  PduId
                    //   8B  LocalCounter
                    //   8B  RemoteCounter   (for replay protection across restarts)
                    //   1B  HasAny flag
                    static constexpr size_t cEntryBytes = 19u;
                    size_t pos = result.size();
                    result.resize(pos + cEntryBytes);
                    uint16_t pdu = kv.first;
                    uint64_t local = kv.second.LocalCounter;
                    uint64_t remote = kv.second.RemoteCounter;
                    uint8_t hasAny = kv.second.HasAny ? 1u : 0u;
                    std::memcpy(result.data() + pos,      &pdu,    2);
                    std::memcpy(result.data() + pos + 2,  &local,  8);
                    std::memcpy(result.data() + pos + 10, &remote, 8);
                    result[pos + 18] = hasAny;
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
                        MakeErrorCode(SecOcErrc::kInvalidPayloadLength));
                }

                uint32_t count = 0;
                std::memcpy(&count, data.data(), 4);

                static constexpr size_t cEntryBytes = 19u;
                if (data.size() < 4 + static_cast<size_t>(count) * cEntryBytes)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kInvalidPayloadLength));
                }

                size_t offset = 4;
                for (uint32_t i = 0; i < count; ++i)
                {
                    uint16_t pdu = 0;
                    uint64_t local = 0;
                    uint64_t remote = 0;
                    std::memcpy(&pdu,    data.data() + offset,      2);
                    std::memcpy(&local,  data.data() + offset + 2,  8);
                    std::memcpy(&remote, data.data() + offset + 10, 8);
                    uint8_t hasAny = data[offset + 18];
                    offset += cEntryBytes;

                    auto it = mEntries.find(pdu);
                    if (it != mEntries.end())
                    {
                        it->second.LocalCounter  = local;
                        it->second.RemoteCounter = remote;
                        it->second.HasAny        = (hasAny != 0u);
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
