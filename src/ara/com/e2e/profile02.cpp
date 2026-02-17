/// @file src/ara/com/e2e/profile02.cpp
/// @brief Implementation for E2E Profile 02 (CRC-8H2F, polynomial 0x2F).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./profile02.h"
#include <cstddef>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            // Static member definitions
            constexpr std::size_t Profile02::cTableSize;
            constexpr uint8_t Profile02::cCrcPoly;
            constexpr uint8_t Profile02::cCrcInitial;
            constexpr uint8_t Profile02::cCounterMax;
            constexpr std::size_t Profile02::cHeaderLength;

            volatile std::atomic_bool Profile02::mCrcTableInitialized{false};
            std::array<uint8_t, Profile02::cTableSize> Profile02::mCrcTable{};

            // -----------------------------------------------------------------------
            // CRC table initialization (CRC-8H2F, polynomial 0x2F, MSB-first)
            // -----------------------------------------------------------------------
            void Profile02::initializeCrcTable() noexcept
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

            uint8_t Profile02::crcStep(uint8_t crc, uint8_t byte) noexcept
            {
                return mCrcTable[static_cast<std::size_t>(crc ^ byte)];
            }

            // -----------------------------------------------------------------------
            // CRC computation over: DataID_high, DataID_low, controlByte1,
            //                       controlByte2, payload bytes
            // -----------------------------------------------------------------------
            uint8_t Profile02::computeCrc(
                const std::vector<uint8_t> &payload,
                uint8_t controlByte1,
                uint8_t controlByte2) const noexcept
            {
                uint8_t crc = cCrcInitial;
                // DataID coverage
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                // Header bytes (excluding CRC byte itself)
                crc = crcStep(crc, controlByte1);
                crc = crcStep(crc, controlByte2);
                // Payload
                for (uint8_t byte : payload)
                {
                    crc = crcStep(crc, byte);
                }
                return static_cast<uint8_t>(~crc);
            }

            // -----------------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------------
            Profile02::Profile02() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile02::Profile02(const Profile02Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            // -----------------------------------------------------------------------
            // TryProtect — build 3-byte header, prepend to payload
            //
            // Header:
            //   byte[0] = CRC-8H2F
            //   byte[1] = (DataID_high_nibble << 4) | counter
            //   byte[2] = DataID_low byte
            // -----------------------------------------------------------------------
            bool Profile02::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                // Increment counter (wraps 0..15)
                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint8_t>(mProtectingCounter + 1)
                                         : 0;

                const uint8_t dataIdHighNibble =
                    static_cast<uint8_t>((mConfig.dataId >> 12) & 0x0F);
                const uint8_t controlByte1 =
                    static_cast<uint8_t>((dataIdHighNibble << 4) | (mProtectingCounter & 0x0F));
                const uint8_t controlByte2 =
                    static_cast<uint8_t>(mConfig.dataId & 0xFF);

                const uint8_t crc = computeCrc(unprotectedData, controlByte1, controlByte2);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(crc);
                protectedData.push_back(controlByte1);
                protectedData.push_back(controlByte2);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            // -----------------------------------------------------------------------
            // TryForward — use last checked counter (gateway / bridge use case)
            // -----------------------------------------------------------------------
            bool Profile02::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                const uint8_t dataIdHighNibble =
                    static_cast<uint8_t>((mConfig.dataId >> 12) & 0x0F);
                const uint8_t controlByte1 =
                    static_cast<uint8_t>((dataIdHighNibble << 4) | (mCheckingCounter & 0x0F));
                const uint8_t controlByte2 =
                    static_cast<uint8_t>(mConfig.dataId & 0xFF);

                const uint8_t crc = computeCrc(unprotectedData, controlByte1, controlByte2);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(crc);
                protectedData.push_back(controlByte1);
                protectedData.push_back(controlByte2);
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            // -----------------------------------------------------------------------
            // Check — verify CRC-8H2F and counter sequence
            // -----------------------------------------------------------------------
            CheckStatusType Profile02::Check(
                const std::vector<uint8_t> &protectedData)
            {
                // Minimum: 3 header bytes + at least 1 payload byte
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                const uint8_t receivedCrc = protectedData[0];
                const uint8_t controlByte1 = protectedData[1];
                const uint8_t controlByte2 = protectedData[2];

                // Reconstruct payload
                const std::vector<uint8_t> payload(protectedData.begin() + cHeaderLength,
                                                   protectedData.end());

                const uint8_t computedCrc = computeCrc(payload, controlByte1, controlByte2);
                if (receivedCrc != computedCrc)
                {
                    return CheckStatusType::kWrongCrc;
                }

                const uint8_t receivedCounter = static_cast<uint8_t>(controlByte1 & 0x0F);
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
}         // namespace ara
