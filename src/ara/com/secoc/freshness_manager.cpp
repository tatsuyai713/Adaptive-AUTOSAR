/// @file src/ara/com/secoc/freshness_manager.cpp
/// @brief SecOC Freshness Value Manager implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./freshness_manager.h"
#include <stdexcept>

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            // -----------------------------------------------------------------------
            // Helper: counter ↔ byte vector (little-endian)
            // -----------------------------------------------------------------------
            FreshnessValue FreshnessManager::counterToBytes(uint64_t counter, uint8_t width)
            {
                FreshnessValue result(width, 0);
                for (uint8_t i = 0; i < width && i < 8; ++i)
                {
                    result[i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
                }
                return result;
            }

            uint64_t FreshnessManager::bytesToCounter(const FreshnessValue &fv)
            {
                uint64_t result = 0;
                const uint8_t width = static_cast<uint8_t>(
                    fv.size() < 8 ? fv.size() : 8);
                for (uint8_t i = 0; i < width; ++i)
                {
                    result |= static_cast<uint64_t>(fv[i]) << (i * 8);
                }
                return result;
            }

            // -----------------------------------------------------------------------
            // RegisterPdu
            // -----------------------------------------------------------------------
            bool FreshnessManager::RegisterPdu(PduId pduId, const FreshnessConfig &config)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it != mEntries.end())
                {
                    return false; // already registered
                }
                Entry entry;
                entry.config = config;
                entry.counter = 0;
                mEntries[pduId] = entry;
                return true;
            }

            // -----------------------------------------------------------------------
            // UnregisterPdu
            // -----------------------------------------------------------------------
            void FreshnessManager::UnregisterPdu(PduId pduId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mEntries.erase(pduId);
            }

            // -----------------------------------------------------------------------
            // GetFreshnessValue
            // -----------------------------------------------------------------------
            ara::core::Result<FreshnessValue> FreshnessManager::GetFreshnessValue(
                PduId pduId) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return ara::core::Result<FreshnessValue>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }
                const Entry &entry = it->second;
                return counterToBytes(entry.counter, entry.config.counterWidth);
            }

            // -----------------------------------------------------------------------
            // IncrementCounter
            // -----------------------------------------------------------------------
            ara::core::Result<void> FreshnessManager::IncrementCounter(PduId pduId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }
                Entry &entry = it->second;
                const uint64_t maxVal = entry.config.maxCounter;

                if (maxVal != 0 && entry.counter >= maxVal)
                {
                    if (entry.config.maxCounter == 0)
                    {
                        entry.counter = 0; // wrap around
                    }
                    else
                    {
                        return ara::core::Result<void>::FromError(
                            MakeErrorCode(SecOcErrc::kFreshnessOverflow));
                    }
                }
                else
                {
                    ++entry.counter;
                }
                return {};
            }

            // -----------------------------------------------------------------------
            // VerifyAndUpdate
            // Receiver side: accept received freshness if it is strictly greater
            // than the stored value (within a tolerance window of ±1 for single-lost msg).
            // -----------------------------------------------------------------------
            ara::core::Result<void> FreshnessManager::VerifyAndUpdate(
                PduId pduId,
                const FreshnessValue &receivedFreshness)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }
                Entry &entry = it->second;

                if (receivedFreshness.size() != entry.config.counterWidth)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kInvalidPayloadLength));
                }

                const uint64_t received = bytesToCounter(receivedFreshness);
                const uint64_t current = entry.counter;

                // Accept if received > current (strictly monotonic)
                // or received == current + 1 (next expected value)
                if (received <= current)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kFreshnessCounterFailed));
                }

                // Update stored counter to match received value
                entry.counter = received;
                return {};
            }

            // -----------------------------------------------------------------------
            // GetCounterValue
            // -----------------------------------------------------------------------
            ara::core::Result<uint64_t> FreshnessManager::GetCounterValue(PduId pduId) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return ara::core::Result<uint64_t>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }
                return it->second.counter;
            }

            // -----------------------------------------------------------------------
            // ResetCounter
            // -----------------------------------------------------------------------
            ara::core::Result<void> FreshnessManager::ResetCounter(PduId pduId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }
                it->second.counter = 0;
                return {};
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara
