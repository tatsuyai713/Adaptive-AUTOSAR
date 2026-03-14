/// @file src/ara/com/e2e/profile06.h
/// @brief Declarations for E2E Profile 06 (CRC-32, polynomial 0x04C11DB7).
/// @details AUTOSAR E2E Profile 06 uses standard CRC-32 (polynomial 0x04C11DB7,
///          reflected processing, initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF).
///          Suitable for large payloads requiring strong error detection.
///
///          Header layout (6 bytes prepended):
///          - byte[0..3]: CRC-32 (little-endian)
///          - byte[4]: counter (0x00–0xFF, 8-bit)
///          - byte[5]: DataID low byte
///
///          CRC input: DataID (2 bytes, BE) + counter + payload bytes
///
/// @note AUTOSAR SWS_E2ELibrary Profile 06 reference.

#ifndef PROFILE06_H
#define PROFILE06_H

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
            /// @brief Configuration for E2E Profile 06
            struct Profile06Config
            {
                uint16_t dataId{0x0000};
                uint8_t maxDeltaCounter{1};
            };

            /// @brief E2E Profile 06 (CRC-32 standard, polynomial 0x04C11DB7, reflected).
            class Profile06 : public Profile
            {
            private:
                static constexpr std::size_t cTableSize{256};
                static constexpr uint32_t cCrcPoly{0xEDB88320U}; ///< reflected 0x04C11DB7
                static constexpr uint32_t cCrcInitial{0xFFFFFFFFU};
                static constexpr uint8_t cCounterMax{0xFF};
                static constexpr std::size_t cHeaderLength{6};

                static volatile std::atomic_bool mCrcTableInitialized;
                static std::array<uint32_t, cTableSize> mCrcTable;

                Profile06Config mConfig;
                uint8_t mProtectingCounter{0};
                uint8_t mCheckingCounter{0};

                static void initializeCrcTable() noexcept;
                static uint32_t crcStep(uint32_t crc, uint8_t byte) noexcept;
                uint32_t computeCrc(const std::vector<uint8_t> &payload,
                                    uint8_t counterByte) const noexcept;

            public:
                Profile06() noexcept;
                explicit Profile06(const Profile06Config &config) noexcept;

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
