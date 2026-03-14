/// @file src/ara/com/e2e/profile03.cpp
/// @brief Implementation for E2E Profile 03 (CRC-16/CCITT, polynomial 0x1021).

#include "./profile03.h"
#include <cstddef>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            constexpr std::size_t Profile03::cTableSize;
            constexpr uint16_t Profile03::cCrcPoly;
            constexpr uint16_t Profile03::cCrcInitial;
            constexpr uint8_t Profile03::cCounterMax;
            constexpr std::size_t Profile03::cHeaderLength;

            volatile std::atomic_bool Profile03::mCrcTableInitialized{false};
            std::array<uint16_t, Profile03::cTableSize> Profile03::mCrcTable{};

            void Profile03::initializeCrcTable() noexcept
            {
                for (uint16_t i = 0; i < cTableSize; ++i)
                {
                    uint16_t crc = static_cast<uint16_t>(i << 8);
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        crc = (crc & 0x8000U)
                                  ? static_cast<uint16_t>((crc << 1) ^ cCrcPoly)
                                  : static_cast<uint16_t>(crc << 1);
                    }
                    mCrcTable[i] = crc;
                }
            }

            uint16_t Profile03::crcStep(uint16_t crc, uint8_t byte) noexcept
            {
                uint8_t idx = static_cast<uint8_t>((crc >> 8) ^ byte);
                return static_cast<uint16_t>((crc << 8) ^ mCrcTable[idx]);
            }

            uint16_t Profile03::computeCrc(
                const std::vector<uint8_t> &payload,
                uint8_t counterByte) const noexcept
            {
                uint16_t crc = cCrcInitial;
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                crc = crcStep(crc, counterByte);
                for (uint8_t b : payload)
                {
                    crc = crcStep(crc, b);
                }
                return crc;
            }

            Profile03::Profile03() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile03::Profile03(const Profile03Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            bool Profile03::TryProtect(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                mProtectingCounter = (mProtectingCounter < cCounterMax)
                                         ? static_cast<uint8_t>(mProtectingCounter + 1)
                                         : 0;

                const uint8_t counterByte = mProtectingCounter;
                const uint16_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc >> 8));
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(counterByte);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            bool Profile03::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                const uint8_t counterByte = mCheckingCounter;
                const uint16_t crc = computeCrc(unprotectedData, counterByte);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc >> 8));
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(counterByte);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            CheckStatusType Profile03::Check(
                const std::vector<uint8_t> &protectedData)
            {
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                const uint16_t receivedCrc =
                    static_cast<uint16_t>(
                        (static_cast<uint16_t>(protectedData[0]) << 8) |
                        protectedData[1]);
                const uint8_t receivedCounter = protectedData[2];

                std::vector<uint8_t> payload(
                    protectedData.begin() + static_cast<std::ptrdiff_t>(cHeaderLength),
                    protectedData.end());

                const uint16_t expectedCrc = computeCrc(payload, receivedCounter);

                if (receivedCrc != expectedCrc)
                {
                    return CheckStatusType::kWrongCrc;
                }

                if (receivedCounter == mCheckingCounter)
                {
                    return CheckStatusType::kRepeated;
                }

                int delta = static_cast<int>(receivedCounter) -
                            static_cast<int>(mCheckingCounter);
                if (delta < 0)
                {
                    delta += (cCounterMax + 1);
                }

                mCheckingCounter = receivedCounter;

                if (delta > mConfig.maxDeltaCounter)
                {
                    return CheckStatusType::kWrongSequence;
                }

                return CheckStatusType::kOk;
            }
        }
    }
}
