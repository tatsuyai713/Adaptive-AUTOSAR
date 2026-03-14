/// @file src/ara/com/e2e/profile07.h
/// @brief Declarations for E2E Profile 07 (CRC-64/ECMA-182).
/// @details AUTOSAR E2E Profile 07 provides end-to-end communication protection
///          using a 64-bit CRC (CRC-64/ECMA-182, polynomial 0x42F0E1EBA9EA3693,
///          reflected processing, poly 0xC96C5795D7870F42 in LSB-first form).
///          Profile 07 is designed for large payloads (up to 4 GB) where
///          32-bit CRC no longer provides sufficient residual error detection
///          probability over high-volume communication channels.
///
///          Header layout (16 bytes prepended):
///          - byte[0-7]:   CRC-64/ECMA-182 (little-endian)
///          - byte[8-9]:   Counter (little-endian, 16-bit, 0x0000-0xFFFE, wrap)
///          - byte[10-13]: DataID (little-endian, 32-bit)
///          - byte[14-15]: Reserved (always 0x00)
///
///          CRC computation input:
///          - DataID (4 bytes, little-endian) + Counter (2 bytes, little-endian)
///            + all payload bytes
///          - Initial value: 0x0000000000000000, final XOR: 0x0000000000000000
///
///          Reference: AUTOSAR SWS_E2ELibrary Profile 07
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PROFILE07_H
#define PROFILE07_H

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
            /// @brief Configuration for E2E Profile 07
            struct Profile07Config
            {
                /// @brief 32-bit data identifier (SWS_E2ELibrary Profile 07)
                uint32_t dataId{0x00000000U};

                /// @brief Maximum counter jump allowed (lost-message tolerance).
                ///        Valid range: 1-65534 (16-bit counter).
                uint16_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 07 (CRC-64/ECMA-182, polynomial 0x42F0E1EBA9EA3693).
            /// @details Provides the strongest protection in the AUTOSAR E2E library,
            ///          suitable for arbitrarily large payloads. Uses the ECMA-182 64-bit
            ///          CRC (same polynomial as in the CRC-64/XZ variant but without
            ///          the input/output XOR typically used in XZ/GO variants).
            ///
            ///          Header (16 bytes prepended):
            ///          - byte[0-7]:   CRC-64/ECMA-182 (64-bit LE)
            ///          - byte[8-9]:   Counter (16-bit LE, 0x0000-0xFFFE)
            ///          - byte[10-13]: DataID (32-bit LE)
            ///          - byte[14-15]: Reserved (0x0000)
            class Profile07 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                /// @brief Reflected polynomial of CRC-64/ECMA-182
                ///        Normal polynomial: 0x42F0E1EBA9EA3693
                ///        Reflected:         0xC96C5795D7870F42
                static constexpr uint64_t cReflectedPoly{0xC96C5795D7870F42ULL};
                static constexpr uint16_t cCounterMax{0xFFFEU};
                static constexpr std::size_t cHeaderLength{16};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint64_t, cTableSize> mCrcTable;

                Profile07Config mConfig;
                uint16_t mProtectingCounter{0};
                uint16_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint64_t crcStep(uint64_t crc, uint8_t byte) noexcept;
                uint64_t computeCrc(const std::vector<uint8_t> &payload,
                                    uint16_t counter) const noexcept;

            public:
                Profile07() noexcept;
                explicit Profile07(const Profile07Config &config) noexcept;

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

#endif // PROFILE07_H
