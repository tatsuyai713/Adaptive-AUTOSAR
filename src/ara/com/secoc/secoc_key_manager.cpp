/// @file src/ara/com/secoc/secoc_key_manager.cpp
/// @brief SecOC Key Provisioning Manager implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./secoc_key_manager.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            SecOcKeyManager::SecOcKeyManager(
                ara::crypto::KeyStorageProvider &keyStorage)
                : mKeyStorage{keyStorage}
            {
            }

            ara::core::Result<void> SecOcKeyManager::RegisterKey(
                PduId pduId,
                const std::string &keySlotId)
            {
                if (keySlotId.empty())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kConfigurationError));
                }

                std::lock_guard<std::mutex> _lock{mMutex};

                if (mEntries.count(pduId) != 0U)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kConfigurationError));
                }

                // Validate the slot exists before accepting the binding
                const auto cSlotResult{mKeyStorage.GetSlot(keySlotId)};
                if (!cSlotResult.HasValue())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                KeyEntry _entry;
                _entry.slotId = keySlotId;
                mEntries.emplace(pduId, std::move(_entry));
                return ara::core::Result<void>::FromValue();
            }

            ara::core::Result<void> SecOcKeyManager::UnregisterKey(PduId pduId)
            {
                std::lock_guard<std::mutex> _lock{mMutex};

                if (mEntries.count(pduId) == 0U)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                mEntries.erase(pduId);
                return ara::core::Result<void>::FromValue();
            }

            ara::core::Result<std::vector<std::uint8_t>>
            SecOcKeyManager::GetKey(PduId pduId)
            {
                std::lock_guard<std::mutex> _lock{mMutex};

                auto _it{mEntries.find(pduId)};
                if (_it == mEntries.end())
                {
                    return ara::core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                KeyEntry &_entry{_it->second};
                if (!_entry.cachedKey.empty())
                {
                    return ara::core::Result<std::vector<std::uint8_t>>::FromValue(
                        _entry.cachedKey);
                }

                // Load from storage
                auto _loadResult{loadKey(_entry.slotId)};
                if (!_loadResult.HasValue())
                {
                    return _loadResult;
                }

                _entry.cachedKey = _loadResult.Value();
                return ara::core::Result<std::vector<std::uint8_t>>::FromValue(
                    _entry.cachedKey);
            }

            ara::core::Result<void> SecOcKeyManager::RefreshKey(PduId pduId)
            {
                std::lock_guard<std::mutex> _lock{mMutex};

                auto _it{mEntries.find(pduId)};
                if (_it == mEntries.end())
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                KeyEntry &_entry{_it->second};
                auto _loadResult{loadKey(_entry.slotId)};
                if (!_loadResult.HasValue())
                {
                    return ara::core::Result<void>::FromError(
                        _loadResult.Error());
                }

                _entry.cachedKey = std::move(_loadResult.Value());
                return ara::core::Result<void>::FromValue();
            }

            bool SecOcKeyManager::IsKeyRegistered(PduId pduId) const
            {
                std::lock_guard<std::mutex> _lock{mMutex};
                return mEntries.count(pduId) != 0U;
            }

            ara::core::Result<std::vector<std::uint8_t>>
            SecOcKeyManager::loadKey(const std::string &slotId)
            {
                const auto cSlotResult{mKeyStorage.GetSlot(slotId)};
                if (!cSlotResult.HasValue())
                {
                    return ara::core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                const ara::crypto::KeySlot *_slot{cSlotResult.Value()};
                if (_slot == nullptr || _slot->IsEmpty())
                {
                    return ara::core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                const auto cKeyResult{_slot->GetKeyMaterial()};
                if (!cKeyResult.HasValue())
                {
                    return ara::core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kKeyNotFound));
                }

                return ara::core::Result<std::vector<std::uint8_t>>::FromValue(
                    cKeyResult.Value());
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara
