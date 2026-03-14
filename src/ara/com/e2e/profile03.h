/// @file src/ara/com/e2e/profile03.h
/// @brief Declarations for E2E Profile 03 (CRC-16/CCITT, polynomial 0x1021).
/// @details AUTOSAR E2E Profile 03 uses CRC-16/CCITT (polynomial 0x1021,
///          MSB-first, initial value 0xFFFF). Designed for medium-length
///          data elements (up to 4096 bytes).
///
///          Header layout (4 bytes prepended):
///          - byte[0]: CRC-16 high byte
///          - byte[1]: CRC-16 low byte
///          - byte[2]: counter (0x00–0x0F)
///          - byte[3]: DataID low byte
///
///          CRC input: DataID_high, DataID_low, counter, payload bytes
///
/// @note AUTOSAR SWS_E2ELibrary Profile 03 reference.

#ifndef PROFILE03_H
#define PROFILE03_H

#include <array>
#include <atomic>
#include <cstdint>
#include "./profile.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            /// @brief Configuration for E2E Profile 03
            struct Profile03Config
            {
                uint16_t dataId{0x0000};
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 03 (CRC-16/CCITT, polynomial 0x1021, MSB-first).
            class Profile03 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                static constexpr uint16_t cCrcPoly{0x1021};
                static constexpr uint16_t cCrcInitial{0xFFFF};
                static constexpr uint8_t cCounterMax{0x0F};
                static constexpr std::size_t cHeaderLength{4};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint16_t, cTableSize> mCrcTable;

                Profile03Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint16_t crcStep(uint16_t crc, uint8_t byte) noexcept;
                uint16_t computeCrc(const std::vector<uint8_t> &payload,
                                    uint8_t counterByte) const noexcept;

            public:
                Profile03() noexcept;
                explicit Profile03(const Profile03Config &config) noexcept;

                bool TryProtect(
                    const std::vector<uint8_t> &unprotectedData,
                    std::vector<uint8_t> &protectedData) override;

                bool TryForward(
                    const std::vector<uint8_t> &unprotectedData,
                    std::vector<uint8_t> &protectedData) override;

                CheckStatusType Check(
                    const std::vector<uint8_t> &protectedData) override;
            };
        }
    }
}

#endif
