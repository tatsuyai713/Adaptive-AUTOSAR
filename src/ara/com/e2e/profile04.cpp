/// @file src/ara/com/e2e/profile04.cpp
/// @brief Implementation for E2E Profile 04 (CRC-32/AUTOSAR).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./profile04.h"
#include <algorithm>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            constexpr std::size_t Profile04::cTableSize;
            constexpr uint32_t Profile04::cReflectedPoly;
            constexpr uint8_t Profile04::cCounterMax;
            constexpr std::size_t Profile04::cHeaderLength;

            volatile std::atomic_bool Profile04::mCrcTableInitialized{false};
            std::array<uint32_t, Profile04::cTableSize> Profile04::mCrcTable{};

            // -----------------------------------------------------------------------
            // CRC-32/AUTOSAR table (reflected polynomial 0xF4ACFB13)
            // Reflected polynomial: 0xC8DF352F (LSB-first processing)
            // -----------------------------------------------------------------------
            void Profile04::initializeCrcTable() noexcept
            {
                for (uint32_t i = 0; i < cTableSize; ++i)
                {
                    uint32_t crc = i;
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        if (crc & 0x00000001U)
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

            uint32_t Profile04::crcStep(uint32_t crc, uint8_t byte) noexcept
            {
                const auto idx = static_cast<uint8_t>((crc ^ byte) & 0xFF);
                return static_cast<uint32_t>((crc >> 8) ^ mCrcTable[idx]);
            }

            // -----------------------------------------------------------------------
            // Compute CRC-32/AUTOSAR over DataID(2B big-endian) + counterByte + payload
            // Initial value: 0xFFFFFFFF, final XOR: 0xFFFFFFFF
            // -----------------------------------------------------------------------
            uint32_t Profile04::computeCrc(
                const std::vector<uint8_t> &payload,
                uint8_t counterByte) const noexcept
            {
                uint32_t crc = 0xFFFFFFFFU; // CRC-32/AUTOSAR initial value
                // DataID big-endian
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Counter byte
                crc = crcStep(crc, counterByte);
                // DataID low byte (additional input per AUTOSAR profile 04)
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Payload
                for (uint8_t b : payload)
                {
                    crc = crcStep(crc, b);
                }
                // CRC-32/AUTOSAR: final XOR with 0xFFFFFFFF
                return crc ^ 0xFFFFFFFFU;
            }

            // -----------------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------------
            Profile04::Profile04() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile04::Profile04(const Profile04Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            // -----------------------------------------------------------------------
            // TryProtect — build 6-byte header
            //   byte[0-3] = CRC32 (LE), byte[4] = counter, byte[5] = DataID_low
            // -----------------------------------------------------------------------
            bool Profile04::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint8_t>(mProtectingCounter + 1)
                                         : 0;

                const uint8_t counterByte = static_cast<uint8_t>(mProtectingCounter & 0x0F);
                const uint32_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                // CRC-32 little-endian (4 bytes)
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
                // Counter and DataID low
                protectedData.push_back(counterByte);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Payload
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            // -----------------------------------------------------------------------
            // TryForward — gateway mode: reuse last checked counter
            // -----------------------------------------------------------------------
            bool Profile04::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                const uint8_t counterByte = static_cast<uint8_t>(mCheckingCounter & 0x0F);
                const uint32_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
                protectedData.push_back(counterByte);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            // -----------------------------------------------------------------------
            // Check — verify CRC-32/AUTOSAR and counter
            // -----------------------------------------------------------------------
            CheckStatusType Profile04::Check(
                const std::vector<uint8_t> &protectedData)
            {
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                // Reconstruct CRC from LE bytes
                const uint32_t receivedCrc =
                    static_cast<uint32_t>(protectedData[0]) |
                    (static_cast<uint32_t>(protectedData[1]) << 8) |
                    (static_cast<uint32_t>(protectedData[2]) << 16) |
                    (static_cast<uint32_t>(protectedData[3]) << 24);
                const uint8_t counterByte = protectedData[4];
                // byte[5] = DataID low (not checked separately; used in CRC computation)

                const std::vector<uint8_t> payload(
                    protectedData.begin() + static_cast<std::ptrdiff_t>(cHeaderLength),
                    protectedData.end());

                const uint32_t computedCrc = computeCrc(payload, counterByte);
                if (receivedCrc != computedCrc)
                {
                    return CheckStatusType::kWrongCrc;
                }

                const uint8_t receivedCounter = static_cast<uint8_t>(counterByte & 0x0F);
                const auto delta = static_cast<int8_t>(receivedCounter - mCheckingCounter);

                CheckStatusType result;
                if (delta == 0)
                {
                    result = CheckStatusType::kRepeated;
                }
                else if (delta < 0)
                {
                    result = CheckStatusType::kWrongSequence;
                }
                else if (delta > static_cast<int8_t>(mConfig.maxDeltaCounter))
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
