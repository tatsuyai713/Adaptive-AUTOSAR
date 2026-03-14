/// @file src/ara/com/e2e/profile07.cpp
/// @brief Implementation for E2E Profile 07 (CRC-64/ECMA-182).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./profile07.h"
#include <algorithm>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            constexpr std::size_t Profile07::cTableSize;
            constexpr uint64_t Profile07::cReflectedPoly;
            constexpr uint16_t Profile07::cCounterMax;
            constexpr std::size_t Profile07::cHeaderLength;

            volatile std::atomic_bool Profile07::mCrcTableInitialized{false};
            std::array<uint64_t, Profile07::cTableSize> Profile07::mCrcTable{};

            // -----------------------------------------------------------------------
            // CRC-64/ECMA-182 table (reflected polynomial 0xC96C5795D7870F42)
            // Normal polynomial 0x42F0E1EBA9EA3693, reflected for LSB-first processing.
            // Initial value: 0x0000000000000000, final XOR: 0x0000000000000000
            // -----------------------------------------------------------------------
            void Profile07::initializeCrcTable() noexcept
            {
                for (uint32_t i = 0; i < cTableSize; ++i)
                {
                    uint64_t crc = static_cast<uint64_t>(i);
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        if (crc & 0x0000000000000001ULL)
                        {
                            crc = (crc >> 1) ^ cReflectedPoly;
                        }
                        else
                        {
                            crc >>= 1;
                        }
                    }
                    mCrcTable[i] = crc;
                }
            }

            uint64_t Profile07::crcStep(uint64_t crc, uint8_t byte) noexcept
            {
                const auto idx = static_cast<uint8_t>((crc ^ static_cast<uint64_t>(byte)) & 0xFFU);
                return static_cast<uint64_t>((crc >> 8) ^ mCrcTable[idx]);
            }

            // -----------------------------------------------------------------------
            // Compute CRC-64/ECMA-182 over:
            //   DataID (4 bytes LE) + Counter (2 bytes LE) + payload bytes
            // Initial value: 0x0000000000000000, final XOR: none
            // -----------------------------------------------------------------------
            uint64_t Profile07::computeCrc(
                const std::vector<uint8_t> &payload,
                uint16_t counter) const noexcept
            {
                uint64_t crc = 0x0000000000000000ULL;
                // DataID (LE 4 bytes)
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFFU));
                crc = crcStep(crc, static_cast<uint8_t>((mConfig.dataId >> 8) & 0xFFU));
                crc = crcStep(crc, static_cast<uint8_t>((mConfig.dataId >> 16) & 0xFFU));
                crc = crcStep(crc, static_cast<uint8_t>((mConfig.dataId >> 24) & 0xFFU));
                // Counter (LE 2 bytes)
                crc = crcStep(crc, static_cast<uint8_t>(counter & 0xFFU));
                crc = crcStep(crc, static_cast<uint8_t>((counter >> 8) & 0xFFU));
                // Payload
                for (uint8_t b : payload)
                {
                    crc = crcStep(crc, b);
                }
                return crc;
            }

            // -----------------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------------
            Profile07::Profile07() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile07::Profile07(const Profile07Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            // -----------------------------------------------------------------------
            // TryProtect — build 16-byte header
            //   byte[0-7]   = CRC64 (LE)
            //   byte[8-9]   = Counter (LE 16-bit)
            //   byte[10-13] = DataID (LE 32-bit)
            //   byte[14-15] = Reserved (0x00)
            // -----------------------------------------------------------------------
            bool Profile07::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint16_t>(mProtectingCounter + 1U)
                                         : 0U;

                const uint16_t counter = mProtectingCounter;
                const uint64_t crc = computeCrc(unprotectedData, counter);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());

                // CRC-64 little-endian (8 bytes)
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 32) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 40) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 48) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 56) & 0xFFU));
                // Counter (LE 2 bytes)
                protectedData.push_back(static_cast<uint8_t>(counter & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((counter >> 8) & 0xFFU));
                // DataID (LE 4 bytes)
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 8) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 16) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 24) & 0xFFU));
                // Reserved (2 bytes)
                protectedData.push_back(0x00U);
                protectedData.push_back(0x00U);
                // Payload
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            // -----------------------------------------------------------------------
            // TryForward — gateway mode: reuse last checked counter
            // -----------------------------------------------------------------------
            bool Profile07::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                const uint16_t counter = mCheckingCounter;
                const uint64_t crc = computeCrc(unprotectedData, counter);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());

                protectedData.push_back(static_cast<uint8_t>(crc & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 32) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 40) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 48) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((crc >> 56) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>(counter & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((counter >> 8) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 8) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 16) & 0xFFU));
                protectedData.push_back(static_cast<uint8_t>((mConfig.dataId >> 24) & 0xFFU));
                protectedData.push_back(0x00U);
                protectedData.push_back(0x00U);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            // -----------------------------------------------------------------------
            // Check — verify CRC-64/ECMA-182 and 16-bit counter sequence
            // -----------------------------------------------------------------------
            CheckStatusType Profile07::Check(
                const std::vector<uint8_t> &protectedData)
            {
                if (protectedData.size() < cHeaderLength + 1U)
                {
                    return CheckStatusType::kNoNewData;
                }

                // Reconstruct CRC-64 from LE bytes
                const uint64_t receivedCrc =
                    static_cast<uint64_t>(protectedData[0]) |
                    (static_cast<uint64_t>(protectedData[1]) << 8) |
                    (static_cast<uint64_t>(protectedData[2]) << 16) |
                    (static_cast<uint64_t>(protectedData[3]) << 24) |
                    (static_cast<uint64_t>(protectedData[4]) << 32) |
                    (static_cast<uint64_t>(protectedData[5]) << 40) |
                    (static_cast<uint64_t>(protectedData[6]) << 48) |
                    (static_cast<uint64_t>(protectedData[7]) << 56);

                const uint16_t receivedCounter =
                    static_cast<uint16_t>(
                        static_cast<uint16_t>(protectedData[8]) |
                        (static_cast<uint16_t>(protectedData[9]) << 8));

                // byte[10-13] = DataID (not re-verified; already in CRC computation)
                // byte[14-15] = Reserved (ignored)

                const std::vector<uint8_t> payload(
                    protectedData.begin() + static_cast<std::ptrdiff_t>(cHeaderLength),
                    protectedData.end());

                const uint64_t computedCrc = computeCrc(payload, receivedCounter);
                if (receivedCrc != computedCrc)
                {
                    return CheckStatusType::kWrongCrc;
                }

                // 16-bit counter delta check (unsigned difference with wrap tolerance)
                const uint32_t delta =
                    (static_cast<uint32_t>(receivedCounter) -
                     static_cast<uint32_t>(mCheckingCounter) +
                     static_cast<uint32_t>(cCounterMax) + 1U) %
                    (static_cast<uint32_t>(cCounterMax) + 1U);

                CheckStatusType result;
                if (delta == 0U)
                {
                    result = CheckStatusType::kRepeated;
                }
                else if (delta > static_cast<uint32_t>(mConfig.maxDeltaCounter))
                {
                    result = CheckStatusType::kWrongSequence;
                }
                else
                {
                    result = CheckStatusType::kOk;
                }

                mCheckingCounter = receivedCounter;
                return result;
            }

        } // namespace e2e
    }     // namespace com
} // namespace ara
