/// @file src/ara/com/e2e/profile05.h
/// @brief Declarations for E2E Profile 05 (CRC-16/ARC, polynomial 0x8005).
/// @details AUTOSAR E2E Profile 05 provides end-to-end communication protection
///          using a 16-bit CRC (CRC-16/ARC, polynomial 0x8005, reflected processing).
///          Profile 05 is designed for larger payloads (up to 4096 bytes) where
///          8-bit CRC no longer provides sufficient Hamming distance.
///
///          Header layout (3 bytes prepended):
///          - byte[0]: CRC-16 low byte (little-endian)
///          - byte[1]: CRC-16 high byte (little-endian)
///          - byte[2]: counter (0x00–0x0F, lower nibble)
///
///          CRC computation input:
///          - DataID (2 bytes, big-endian) + counter byte + payload
///
///          Reference: AUTOSAR SWS_E2ELibrary Profile 05
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PROFILE05_H
#define PROFILE05_H

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
            /// @brief Configuration for E2E Profile 05
            struct Profile05Config
            {
                /// @brief 16-bit data identifier
                uint16_t dataId{0x0000};

                /// @brief Maximum counter jump allowed (lost-message tolerance)
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 05 (CRC-16/ARC, polynomial 0x8005, LSB-first).
            /// @details Provides stronger protection than Profile 01/02 for payloads
            ///          up to 4096 bytes. Uses reflected (LSB-first) CRC-16 processing.
            ///
            ///          Header (3 bytes prepended):
            ///          - byte[0]: CRC16_L (low byte of CRC-16, LE)
            ///          - byte[1]: CRC16_H (high byte of CRC-16, LE)
            ///          - byte[2]: counter (4 bits, 0x00–0x0F)
            class Profile05 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                static constexpr uint16_t cCrcPoly{0x8005}; ///< CRC-16/ARC polynomial (reflected)
                static constexpr uint8_t cCounterMax{0x0f};
                static constexpr std::size_t cHeaderLength{3};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint16_t, cTableSize> mCrcTable;

                Profile05Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint16_t crcStep(uint16_t crc, uint8_t byte) noexcept;
                uint16_t computeCrc(const std::vector<uint8_t> &payload,
                                    uint8_t counterByte) const noexcept;

            public:
                Profile05() noexcept;
                explicit Profile05(const Profile05Config &config) noexcept;

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

#endif // PROFILE05_H
