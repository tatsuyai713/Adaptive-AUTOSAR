/// @file src/ara/com/secoc/freshness_manager.cpp
/// @brief SecOC Freshness Value Manager implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./freshness_manager.h"

#include <fstream>
#include <sstream>
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

            // -----------------------------------------------------------------------
            // SaveToFile
            // -----------------------------------------------------------------------
            ara::core::Result<void> FreshnessManager::SaveToFile(
                const std::string &filePath) const
            {
                std::lock_guard<std::mutex> lock(mMutex);

                std::ofstream ofs{filePath};
                if (!ofs)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }

                for (const auto &kv : mEntries)
                {
                    ofs << static_cast<uint32_t>(kv.first) << " "
                        << kv.second.counter << "\n";
                }

                if (!ofs)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }

                return ara::core::Result<void>::FromValue();
            }

            // -----------------------------------------------------------------------
            // LoadFromFile
            // -----------------------------------------------------------------------
            ara::core::Result<void> FreshnessManager::LoadFromFile(
                const std::string &filePath)
            {
                std::ifstream ifs{filePath};
                if (!ifs)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kNotInitialized));
                }

                std::lock_guard<std::mutex> lock(mMutex);

                std::string line;
                while (std::getline(ifs, line))
                {
                    if (line.empty() || line[0] == '#')
                    {
                        continue;
                    }

                    std::istringstream iss{line};
                    uint32_t pduIdRaw{0};
                    uint64_t counter{0};
                    if (!(iss >> pduIdRaw >> counter))
                    {
                        return ara::core::Result<void>::FromError(
                            MakeErrorCode(SecOcErrc::kConfigurationError));
                    }

                    const PduId pduId{static_cast<PduId>(pduIdRaw)};
                    auto it = mEntries.find(pduId);
                    if (it != mEntries.end())
                    {
                        it->second.counter = counter;
                    }
                    // Unknown PDU IDs are silently ignored
                }

                return ara::core::Result<void>::FromValue();
            }

            // -----------------------------------------------------------------------
            // SetOverflowWarningCallback (SWS_SecOC_00204)
            // -----------------------------------------------------------------------
            void FreshnessManager::SetOverflowWarningCallback(
                std::function<void(PduId, uint64_t, uint64_t)> callback)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mOverflowWarningCallback = std::move(callback);
            }

            // -----------------------------------------------------------------------
            // IsNearOverflow (SWS_SecOC_00204)
            // -----------------------------------------------------------------------
            bool FreshnessManager::IsNearOverflow(PduId pduId) const
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mEntries.find(pduId);
                if (it == mEntries.end())
                {
                    return false;
                }
                const Entry &entry = it->second;
                if (entry.config.maxCounter == 0U)
                {
                    return false; // no limit configured
                }
                const double ratio =
                    static_cast<double>(entry.counter) /
                    static_cast<double>(entry.config.maxCounter);
                return ratio >= cOverflowWarningRatio;
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara
