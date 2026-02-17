/// @file src/ara/crypto/key_slot.h
/// @brief Key slot for secure key material storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef KEY_SLOT_H
#define KEY_SLOT_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Type of cryptographic key stored in a slot.
        enum class KeySlotType : std::uint32_t
        {
            kSymmetric = 0,
            kRsaPublic = 1,
            kRsaPrivate = 2,
            kEccPublic = 3,
            kEccPrivate = 4
        };

        /// @brief Metadata describing a key slot.
        struct KeySlotMetadata
        {
            std::string SlotId;
            KeySlotType Type;
            std::uint32_t KeySizeBits;
            bool Exportable;
        };

        /// @brief A named container for cryptographic key material with metadata.
        class KeySlot
        {
        public:
            /// @brief Construct a key slot with metadata and initial key material.
            KeySlot(const KeySlotMetadata &metadata,
                    const std::vector<std::uint8_t> &keyMaterial);

            /// @brief Get the slot metadata.
            const KeySlotMetadata &GetMetadata() const noexcept;

            /// @brief Retrieve key material (only if slot is marked Exportable).
            core::Result<std::vector<std::uint8_t>> GetKeyMaterial() const;

            /// @brief Check whether the slot contains key material.
            bool IsEmpty() const noexcept;

            /// @brief Replace key material in the slot.
            core::Result<void> Update(const std::vector<std::uint8_t> &keyMaterial);

            /// @brief Remove all key material from the slot.
            void Clear();

        private:
            mutable std::mutex mMutex;
            KeySlotMetadata mMetadata;
            std::vector<std::uint8_t> mKeyMaterial;
        };
    }
}

#endif
