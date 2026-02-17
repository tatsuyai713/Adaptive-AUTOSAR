/// @file src/ara/com/e2e/profile01.h
/// @brief Declarations for E2E Profile 01 (CRC-8 SAE-J1850).
/// @details AUTOSAR E2E Profile 01 provides end-to-end communication protection
///          using an 8-bit CRC (polynomial 0x1D, SAE-J1850 compatible).
///          Header layout: byte[0]=CRC8, byte[1]=DataID_nibble[7:4] | Counter[3:0].
///          This file is part of the Adaptive AUTOSAR educational implementation.
///
/// @note AUTOSAR SWS_E2ELibrary Profile 01 reference:
///       - CRC polynomial: 0x1D (SAE-J1850)
///       - Counter: 4 bits, wraps at 14 (0x00–0x0E)
///       - DataID: 16-bit configurable data identifier
///       - Max protected data length: 240 bytes

#ifndef PROFILE01_H
#define PROFILE01_H

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
            /// @brief Configuration for E2E Profile 01
            struct Profile01Config
            {
                /// @brief 16-bit data identifier (unique per signal / event)
                uint16_t dataId{0x0000};

                /// @brief Maximum allowed counter delta between consecutive messages.
                /// @details Messages with counter delta > maxDeltaCounter are marked kWrongSequence.
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 01 implementation (CRC-8 SAE-J1850 / polynomial 0x1D)
            /// @details Implements TryProtect / TryForward / Check per AUTOSAR E2E Profile 01.
            ///          Header (2 bytes prepended):
            ///          - byte[0]: CRC-8 over [DataID_high, DataID_low, header_byte1, data...]
            ///          - byte[1]: DataID nibble (bits [7:4]) | Counter (bits [3:0])
            class Profile01 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                static constexpr uint8_t cCrcPoly{0x1d};     ///< SAE-J1850 polynomial
                static constexpr uint8_t cCrcInitial{0xff};
                static constexpr uint8_t cCounterMax{0x0e};  ///< counter wraps 0..14
                static constexpr std::size_t cHeaderLength{2};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint8_t, cTableSize> mCrcTable;

                Profile01Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint8_t crcStep(uint8_t crc, uint8_t byte) noexcept;

                /// @brief Compute CRC-8 over DataID bytes + header control byte + payload
                uint8_t computeCrc(const std::vector<uint8_t> &data,
                                   uint8_t controlByte) const noexcept;

            public:
                /// @brief Construct Profile 01 with default configuration (DataID=0)
                Profile01() noexcept;

                /// @brief Construct Profile 01 with explicit configuration
                /// @param config Profile 01 configuration (DataID, maxDeltaCounter)
                explicit Profile01(const Profile01Config &config) noexcept;

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

#endif // PROFILE01_H
