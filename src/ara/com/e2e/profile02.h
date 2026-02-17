/// @file src/ara/com/e2e/profile02.h
/// @brief Declarations for E2E Profile 02 (CRC-8H2F, polynomial 0x2F).
/// @details AUTOSAR E2E Profile 02 provides end-to-end protection for longer
///          messages using CRC-8H2F (polynomial 0x2F), which has better Hamming
///          distance properties than SAE-J1850 for data lengths beyond 119 bits.
///
///          Header layout (3 bytes prepended):
///          - byte[0]: CRC-8H2F (over DataID + header bytes + payload)
///          - byte[1]: counter (0x00–0x0F, lower nibble, upper nibble = DataID nibble)
///          - byte[2]: DataID low byte
///
/// @note AUTOSAR SWS_E2ELibrary Profile 02 reference:
///       - CRC polynomial: 0x2F (CRC-8H2F)
///       - CRC initial value: 0xFF, XOR: ~result
///       - Counter: 4 bits, wraps at 15 (0x00–0x0F)
///       - DataID: 16-bit configurable identifier
///       - Max protected data length: 240 bytes
///
///       This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PROFILE02_H
#define PROFILE02_H

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
            /// @brief Configuration for E2E Profile 02
            struct Profile02Config
            {
                /// @brief 16-bit data identifier (unique per signal / event)
                uint16_t dataId{0x0000};

                /// @brief Maximum allowed counter delta (lost messages tolerance)
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 02 implementation (CRC-8H2F, polynomial 0x2F)
            /// @details Improved Hamming distance over Profile 01 for messages up to 240 bytes.
            ///          Header (3 bytes prepended):
            ///          - byte[0]: CRC-8H2F
            ///          - byte[1]: (DataID_nibble[3:0] << 4) | counter[3:0]
            ///          - byte[2]: DataID low byte (additional DataID coverage)
            ///
            ///          CRC input order:
            ///          DataID_high, DataID_low, byte[1], byte[2], payload...
            class Profile02 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                static constexpr uint8_t cCrcPoly{0x2f};     ///< CRC-8H2F polynomial
                static constexpr uint8_t cCrcInitial{0xff};
                static constexpr uint8_t cCounterMax{0x0f};  ///< counter wraps 0..15
                static constexpr std::size_t cHeaderLength{3};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint8_t, cTableSize> mCrcTable;

                Profile02Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint8_t crcStep(uint8_t crc, uint8_t byte) noexcept;

                /// @brief Compute CRC-8H2F over DataID bytes + control bytes + payload
                uint8_t computeCrc(const std::vector<uint8_t> &payload,
                                   uint8_t controlByte1,
                                   uint8_t controlByte2) const noexcept;

            public:
                /// @brief Construct Profile 02 with default configuration (DataID=0)
                Profile02() noexcept;

                /// @brief Construct Profile 02 with explicit configuration
                /// @param config Profile 02 configuration (DataID, maxDeltaCounter)
                explicit Profile02(const Profile02Config &config) noexcept;

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

#endif // PROFILE02_H
