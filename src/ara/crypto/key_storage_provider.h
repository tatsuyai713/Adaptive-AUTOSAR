/// @file src/ara/crypto/key_storage_provider.h
/// @brief Key storage provider for managing collections of key slots.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef KEY_STORAGE_PROVIDER_H
#define KEY_STORAGE_PROVIDER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./key_slot.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Manages a collection of key slots with filesystem persistence.
        class KeyStorageProvider
        {
        public:
            /// @brief Create a new empty key slot.
            core::Result<void> CreateSlot(const KeySlotMetadata &metadata);

            /// @brief Delete a key slot by ID.
            core::Result<void> DeleteSlot(const std::string &slotId);

            /// @brief Get a pointer to a key slot.
            core::Result<KeySlot *> GetSlot(const std::string &slotId);

            /// @brief List all slot IDs.
            std::vector<std::string> ListSlotIds() const;

            /// @brief Store key material into an existing slot.
            core::Result<void> StoreKey(
                const std::string &slotId,
                const std::vector<std::uint8_t> &keyMaterial);

            /// @brief Save all slots to a directory (one file per slot).
            core::Result<void> SaveToDirectory(const std::string &dirPath) const;

            /// @brief Load slots from a directory.
            core::Result<void> LoadFromDirectory(const std::string &dirPath);

        private:
            mutable std::mutex mMutex;
            std::unordered_map<std::string, std::unique_ptr<KeySlot>> mSlots;
        };
    }
}

#endif
