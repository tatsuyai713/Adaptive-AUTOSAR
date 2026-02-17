/// @file src/ara/com/e2e/profile01.cpp
/// @brief Implementation for E2E Profile 01 (CRC-8 SAE-J1850).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./profile01.h"
#include <cstddef>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            // Static member definitions
            constexpr std::size_t Profile01::cTableSize;
            constexpr uint8_t Profile01::cCrcPoly;
            constexpr uint8_t Profile01::cCrcInitial;
            constexpr uint8_t Profile01::cCounterMax;
            constexpr std::size_t Profile01::cHeaderLength;

            volatile std::atomic_bool Profile01::mCrcTableInitialized{false};
            std::array<uint8_t, Profile01::cTableSize> Profile01::mCrcTable{};

            // -----------------------------------------------------------------------
            // CRC table initialization (SAE-J1850, polynomial 0x1D, MSB-first)
            // -----------------------------------------------------------------------
            void Profile01::initializeCrcTable() noexcept
            {
                const uint8_t cMsb{0x80};
                for (uint16_t i = 0; i < cTableSize; ++i)
                {
                    auto crc = static_cast<uint8_t>(i);
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        crc = (crc & cMsb) ? static_cast<uint8_t>((crc << 1) ^ cCrcPoly)
                                           : static_cast<uint8_t>(crc << 1);
                    }
                    mCrcTable[i] = crc;
                }
            }

            uint8_t Profile01::crcStep(uint8_t crc, uint8_t byte) noexcept
            {
                return mCrcTable[static_cast<std::size_t>(crc ^ byte)];
            }

            // -----------------------------------------------------------------------
            // CRC computation
            // Header layout per AUTOSAR E2E Profile 01:
            //   byte[0] = CRC   (computed over DataID + control byte + payload)
            //   byte[1] = controlByte = (DataID_nibble << 4) | counter
            //
            // CRC covers in order:
            //   DataID high byte, DataID low byte, controlByte, payload bytes...
            // -----------------------------------------------------------------------
            uint8_t Profile01::computeCrc(
                const std::vector<uint8_t> &data,
                uint8_t controlByte) const noexcept
            {
                uint8_t crc = cCrcInitial;
                // Include DataID in CRC
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Include control byte (header byte[1])
                crc = crcStep(crc, controlByte);
                // Include payload
                for (uint8_t byte : data)
                {
                    crc = crcStep(crc, byte);
                }
                return static_cast<uint8_t>(~crc);
            }

            // -----------------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------------
            Profile01::Profile01() noexcept : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile01::Profile01(const Profile01Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            // -----------------------------------------------------------------------
            // TryProtect — increment counter, build 2-byte header, prepend to data
            // -----------------------------------------------------------------------
            bool Profile01::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                // Increment counter (wraps 0..14)
                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint8_t>(mProtectingCounter + 1)
                                         : 0;

                // Control byte: DataID nibble (upper 4 bits) | counter (lower 4 bits)
                const uint8_t dataIdNibble =
                    static_cast<uint8_t>((mConfig.dataId & 0xF000) >> 8);
                const uint8_t controlByte =
                    static_cast<uint8_t>(dataIdNibble | (mProtectingCounter & 0x0F));

                const uint8_t crc = computeCrc(unprotectedData, controlByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(crc);
                protectedData.push_back(controlByte);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            // -----------------------------------------------------------------------
            // TryForward — use last checked counter (gateway / bridge use case)
            // -----------------------------------------------------------------------
            bool Profile01::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                const uint8_t dataIdNibble =
                    static_cast<uint8_t>((mConfig.dataId & 0xF000) >> 8);
                const uint8_t controlByte =
                    static_cast<uint8_t>(dataIdNibble | (mCheckingCounter & 0x0F));

                const uint8_t crc = computeCrc(unprotectedData, controlByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(crc);
                protectedData.push_back(controlByte);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                // Keep protect counter aligned with check counter for forwarding nodes.
                mProtectingCounter = mCheckingCounter;
                return true;
            }

            // -----------------------------------------------------------------------
            // Check — verify CRC and counter sequence
            // -----------------------------------------------------------------------
            CheckStatusType Profile01::Check(
                const std::vector<uint8_t> &protectedData)
            {
                // Minimum: CRC byte + control byte + at least 1 payload byte
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                const uint8_t receivedCrc = protectedData[0];
                const uint8_t controlByte = protectedData[1];

                // Reconstruct payload view
                const std::vector<uint8_t> payload(protectedData.begin() + cHeaderLength,
                                                   protectedData.end());

                const uint8_t computedCrc = computeCrc(payload, controlByte);
                if (receivedCrc != computedCrc)
                {
                    return CheckStatusType::kWrongCrc;
                }

                const uint8_t receivedCounter = static_cast<uint8_t>(controlByte & 0x0F);

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
                    // Counter jumped more than allowed — lost messages
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
}         // namespace ara
