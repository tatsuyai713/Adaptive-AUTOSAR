/// @file src/ara/com/e2e/profile04.h
/// @brief Declarations for E2E Profile 04 (CRC-32/AUTOSAR, polynomial 0xF4ACFB13).
/// @details AUTOSAR E2E Profile 04 provides end-to-end communication protection
///          using a 32-bit CRC (CRC-32/AUTOSAR, polynomial 0xF4ACFB13, reflected).
///          Profile 04 is designed for larger payloads where Profile 02 (CRC-8)
///          or Profile 05 (CRC-16) no longer provide sufficient Hamming distance.
///
///          Header layout (6 bytes prepended):
///          - byte[0-3]: CRC-32/AUTOSAR (little-endian, XOR-out = 0xFFFFFFFF)
///          - byte[4]:   counter (0x00-0x0E, lower 4 bits used)
///          - byte[5]:   DataID low byte
///
///          CRC computation input:
///          - DataID[1] (high byte, big-endian first) + DataID[0] (low byte)
///            + counter byte + all payload bytes
///          - Initial value: 0xFFFFFFFF, final XOR: 0xFFFFFFFF
///
///          Reference: AUTOSAR SWS_E2ELibrary Profile 04
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PROFILE04_H
#define PROFILE04_H

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
            /// @brief Configuration for E2E Profile 04
            struct Profile04Config
            {
                /// @brief 16-bit data identifier
                uint16_t dataId{0x0000};

                /// @brief Maximum counter jump allowed (lost-message tolerance)
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 04 (CRC-32/AUTOSAR, polynomial 0xF4ACFB13, reflected).
            /// @details Provides the strongest protection for large payloads.
            ///          Uses the AUTOSAR-specific CRC-32P4 (not the standard CRC-32
            ///          used in Ethernet/ZIP).
            ///
            ///          Header (6 bytes prepended):
            ///          - byte[0-3]: CRC-32/AUTOSAR (32-bit LE, XOR 0xFFFFFFFF)
            ///          - byte[4]:   counter (4 lower bits, 0x00-0x0E)
            ///          - byte[5]:   DataID low byte
            class Profile04 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                /// @brief Reflected polynomial of CRC-32/AUTOSAR (0xF4ACFB13 reflected)
                static constexpr uint32_t cReflectedPoly{0xC8DF352FU};
                static constexpr uint8_t cCounterMax{0x0e};
                static constexpr std::size_t cHeaderLength{6};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint32_t, cTableSize> mCrcTable;

                Profile04Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint32_t crcStep(uint32_t crc, uint8_t byte) noexcept;
                uint32_t computeCrc(const std::vector<uint8_t> &payload,
                                    uint8_t counterByte) const noexcept;

            public:
                Profile04() noexcept;
                explicit Profile04(const Profile04Config &config) noexcept;

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

#endif // PROFILE04_H
