/// @file src/ara/crypto/key_slot.cpp
/// @brief Implementation for key slot.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./key_slot.h"

namespace ara
{
    namespace crypto
    {
        KeySlot::KeySlot(
            const KeySlotMetadata &metadata,
            const std::vector<std::uint8_t> &keyMaterial)
            : mMetadata{metadata},
              mKeyMaterial{keyMaterial}
        {
        }

        const KeySlotMetadata &KeySlot::GetMetadata() const noexcept
        {
            return mMetadata;
        }

        core::Result<std::vector<std::uint8_t>> KeySlot::GetKeyMaterial() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            if (!mMetadata.Exportable)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (mKeyMaterial.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kSlotNotFound));
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(mKeyMaterial);
        }

        bool KeySlot::IsEmpty() const noexcept
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return mKeyMaterial.empty();
        }

        core::Result<void> KeySlot::Update(
            const std::vector<std::uint8_t> &keyMaterial)
        {
            if (keyMaterial.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mKeyMaterial = keyMaterial;
            return core::Result<void>::FromValue();
        }

        void KeySlot::Clear()
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mKeyMaterial.clear();
        }
    }
}
