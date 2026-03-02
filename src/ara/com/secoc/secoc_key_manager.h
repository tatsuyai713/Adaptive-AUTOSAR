/// @file src/ara/com/secoc/secoc_key_manager.h
/// @brief SecOC Key Provisioning Manager.
/// @details Bridges ara::crypto::KeyStorageProvider and SecOC PDU processing.
///          Each PDU is bound to a named key slot.  The manager loads the raw
///          symmetric key bytes on demand and caches them in memory.
///
///          Typical usage (ECU startup):
///          @code
///          ara::crypto::KeyStorageProvider ksp;
///          ksp.LoadFromDirectory("/opt/autosar/keys");
///
///          SecOcKeyManager km(ksp);
///          km.RegisterKey(0x100, "secoc_pdu_100");  // PDU 0x100 → slot name
///
///          auto keyResult = km.GetKey(0x100);
///          // use key bytes to construct SecOcPdu …
///          @endcode
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SECOC_SECOC_KEY_MANAGER_H
#define ARA_COM_SECOC_SECOC_KEY_MANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "../../core/result.h"
#include "../../crypto/key_storage_provider.h"
#include "./freshness_manager.h"
#include "./secoc_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            /// @brief Manages the mapping between SecOC PDU IDs and key slots.
            ///
            /// Thread-safe.  Delegates key material loading to an external
            /// ara::crypto::KeyStorageProvider instance.
            class SecOcKeyManager
            {
            public:
                /// @brief Construct a key manager backed by the given key storage.
                /// @param keyStorage Reference to a configured KeyStorageProvider.
                explicit SecOcKeyManager(ara::crypto::KeyStorageProvider &keyStorage);

                SecOcKeyManager(const SecOcKeyManager &) = delete;
                SecOcKeyManager &operator=(const SecOcKeyManager &) = delete;

                /// @brief Bind a PDU to a named key slot.
                /// @param pduId PDU identifier.
                /// @param keySlotId Key slot name in the associated KeyStorageProvider.
                /// @returns Ok, or kConfigurationError if a binding already exists,
                ///          or kKeyNotFound if the slot does not exist in the provider.
                ara::core::Result<void> RegisterKey(PduId pduId,
                                                    const std::string &keySlotId);

                /// @brief Remove the key binding for a PDU.
                /// @param pduId PDU identifier.
                /// @returns Ok, or kKeyNotFound if the PDU was not registered.
                ara::core::Result<void> UnregisterKey(PduId pduId);

                /// @brief Retrieve the symmetric key bytes for a PDU.
                /// @details Returns the cached key if already loaded; otherwise loads
                ///          from the key storage.
                /// @param pduId PDU identifier.
                /// @returns Key bytes, or kKeyNotFound if the PDU is not registered
                ///          or the slot contains no key material.
                ara::core::Result<std::vector<std::uint8_t>> GetKey(PduId pduId);

                /// @brief Force a key reload from storage (e.g., after key rotation).
                /// @param pduId PDU identifier.
                /// @returns Ok, or kKeyNotFound if PDU is not registered.
                ara::core::Result<void> RefreshKey(PduId pduId);

                /// @brief Check whether a key binding exists for a PDU.
                bool IsKeyRegistered(PduId pduId) const;

            private:
                mutable std::mutex mMutex;
                ara::crypto::KeyStorageProvider &mKeyStorage;

                struct KeyEntry
                {
                    std::string slotId;
                    std::vector<std::uint8_t> cachedKey; ///< Empty = not loaded yet
                };

                std::unordered_map<PduId, KeyEntry> mEntries;

                /// @brief Internal key load from storage (must be called with lock held).
                ara::core::Result<std::vector<std::uint8_t>> loadKey(
                    const std::string &slotId);
            };
        } // namespace secoc
    }     // namespace com
} // namespace ara

#endif // ARA_COM_SECOC_SECOC_KEY_MANAGER_H
