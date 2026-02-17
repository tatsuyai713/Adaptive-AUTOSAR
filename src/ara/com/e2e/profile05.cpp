/// @file src/ara/com/e2e/profile05.cpp
/// @brief Implementation for E2E Profile 05 (CRC-16/ARC).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./profile05.h"
#include <algorithm>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            constexpr std::size_t Profile05::cTableSize;
            constexpr uint16_t Profile05::cCrcPoly;
            constexpr uint8_t Profile05::cCounterMax;
            constexpr std::size_t Profile05::cHeaderLength;

            volatile std::atomic_bool Profile05::mCrcTableInitialized{false};
            std::array<uint16_t, Profile05::cTableSize> Profile05::mCrcTable{};

            // -----------------------------------------------------------------------
            // CRC-16/ARC table (reflected polynomial, LSB-first)
            // -----------------------------------------------------------------------
            void Profile05::initializeCrcTable() noexcept
            {
                // Reflected polynomial 0x8005: reflect(0x8005) = 0xA001
                const uint16_t reflectedPoly = 0xA001U;
                for (uint16_t i = 0; i < cTableSize; ++i)
                {
                    uint16_t crc = i;
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        if (crc & 0x0001U)
                        {
                            crc = static_cast<uint16_t>((crc >> 1) ^ reflectedPoly);
                        }
                        else
                        {
                            crc >>= 1;
                        }
                    }
                    mCrcTable[i] = crc;
                }
            }

            uint16_t Profile05::crcStep(uint16_t crc, uint8_t byte) noexcept
            {
                const auto idx = static_cast<uint8_t>((crc ^ byte) & 0xFF);
                return static_cast<uint16_t>((crc >> 8) ^ mCrcTable[idx]);
            }

            // -----------------------------------------------------------------------
            // Compute CRC-16/ARC over DataID(2B big-endian) + counterByte + payload
            // -----------------------------------------------------------------------
            uint16_t Profile05::computeCrc(
                const std::vector<uint8_t> &payload,
                uint8_t counterByte) const noexcept
            {
                uint16_t crc = 0xFFFF; // CRC-16/ARC initial value
                // DataID big-endian
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Counter byte
                crc = crcStep(crc, counterByte);
                // Payload
                for (uint8_t b : payload)
                {
                    crc = crcStep(crc, b);
                }
                // CRC-16/ARC: no final XOR (XOR value = 0x0000)
                return crc;
            }

            // -----------------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------------
            Profile05::Profile05() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile05::Profile05(const Profile05Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            // -----------------------------------------------------------------------
            // TryProtect — build 3-byte header
            //   byte[0] = CRC16_L, byte[1] = CRC16_H, byte[2] = counter
            // -----------------------------------------------------------------------
            bool Profile05::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint8_t>(mProtectingCounter + 1)
                                         : 0;

                const uint8_t counterByte = static_cast<uint8_t>(mProtectingCounter & 0x0F);
                const uint16_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));        // CRC16_L
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF)); // CRC16_H
                protectedData.push_back(counterByte);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            // -----------------------------------------------------------------------
            // TryForward — use last checked counter (gateway mode)
            // -----------------------------------------------------------------------
            bool Profile05::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty()) return false;

                const uint8_t counterByte = static_cast<uint8_t>(mCheckingCounter & 0x0F);
                const uint16_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
                protectedData.push_back(counterByte);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            // -----------------------------------------------------------------------
            // Check — verify CRC-16 and counter
            // -----------------------------------------------------------------------
            CheckStatusType Profile05::Check(
                const std::vector<uint8_t> &protectedData)
            {
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                const uint16_t receivedCrc =
                    static_cast<uint16_t>(protectedData[0]) |
                    (static_cast<uint16_t>(protectedData[1]) << 8);
                const uint8_t counterByte = protectedData[2];

                const std::vector<uint8_t> payload(
                    protectedData.begin() + static_cast<std::ptrdiff_t>(cHeaderLength),
                    protectedData.end());

                const uint16_t computedCrc = computeCrc(payload, counterByte);
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
