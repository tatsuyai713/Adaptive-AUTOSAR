/// @file src/ara/com/e2e/profile06.cpp
/// @brief Implementation for E2E Profile 06 (CRC-32 standard, reflected).

#include "./profile06.h"
#include <cstddef>

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            constexpr std::size_t Profile06::cTableSize;
            constexpr uint32_t Profile06::cCrcPoly;
            constexpr uint32_t Profile06::cCrcInitial;
            constexpr uint8_t Profile06::cCounterMax;
            constexpr std::size_t Profile06::cHeaderLength;

            volatile std::atomic_bool Profile06::mCrcTableInitialized{false};
            std::array<uint32_t, Profile06::cTableSize> Profile06::mCrcTable{};

            void Profile06::initializeCrcTable() noexcept
            {
                for (uint32_t i = 0; i < cTableSize; ++i)
                {
                    uint32_t crc = i;
                    for (uint8_t bit = 0; bit < 8; ++bit)
                    {
                        crc = (crc & 1U)
                                  ? ((crc >> 1) ^ cCrcPoly)
                                  : (crc >> 1);
                    }
                    mCrcTable[i] = crc;
                }
            }

            uint32_t Profile06::crcStep(uint32_t crc, uint8_t byte) noexcept
            {
                return (crc >> 8) ^ mCrcTable[static_cast<uint8_t>(crc ^ byte)];
            }

            uint32_t Profile06::computeCrc(
                const std::vector<uint8_t> &payload,
                uint8_t counterByte) const noexcept
            {
                uint32_t crc = cCrcInitial;
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId >> 8));
                crc = crcStep(crc, static_cast<uint8_t>(mConfig.dataId & 0xFF));
                crc = crcStep(crc, counterByte);
                for (uint8_t b : payload)
                {
                    crc = crcStep(crc, b);
                }
                return crc ^ 0xFFFFFFFFU;
            }

            Profile06::Profile06() noexcept
                : mConfig{}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            Profile06::Profile06(const Profile06Config &config) noexcept
                : mConfig{config}, mProtectingCounter{0}, mCheckingCounter{0}
            {
                if (!mCrcTableInitialized)
                {
                    mCrcTableInitialized = true;
                    initializeCrcTable();
                }
            }

            bool Profile06::TryProtect(
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

                const uint32_t crc = computeCrc(unprotectedData, mProtectingCounter);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
                protectedData.push_back(mProtectingCounter);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());
                return true;
            }

            bool Profile06::TryForward(
                const std::vector<uint8_t> &unprotectedData,
                std::vector<uint8_t> &protectedData)
            {
                if (unprotectedData.empty())
                {
                    return false;
                }

                const uint32_t crc = computeCrc(unprotectedData, mCheckingCounter);

                protectedData.clear();
                protectedData.reserve(cHeaderLength + unprotectedData.size());
                protectedData.push_back(static_cast<uint8_t>(crc & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
                protectedData.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
                protectedData.push_back(mCheckingCounter);
                protectedData.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));
                protectedData.insert(protectedData.end(),
                                     unprotectedData.begin(), unprotectedData.end());

                mProtectingCounter = mCheckingCounter;
                return true;
            }

            CheckStatusType Profile06::Check(
                const std::vector<uint8_t> &protectedData)
            {
                if (protectedData.size() < cHeaderLength + 1)
                {
                    return CheckStatusType::kNoNewData;
                }

                const uint32_t receivedCrc =
                    static_cast<uint32_t>(protectedData[0]) |
                    (static_cast<uint32_t>(protectedData[1]) << 8) |
                    (static_cast<uint32_t>(protectedData[2]) << 16) |
                    (static_cast<uint32_t>(protectedData[3]) << 24);
                const uint8_t receivedCounter = protectedData[4];

                std::vector<uint8_t> payload(
                    protectedData.begin() + static_cast<std::ptrdiff_t>(cHeaderLength),
                    protectedData.end());

                const uint32_t expectedCrc = computeCrc(payload, receivedCounter);

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
