/// @file src/ara/crypto/hsm_provider.cpp
/// @brief Implementation of HsmProvider (software emulation).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./hsm_provider.h"
#include <algorithm>

namespace ara
{
    namespace crypto
    {
        core::Result<void> HsmProvider::Initialize(
            const std::string &tokenLabel)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mInitialized)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }
            mTokenLabel = tokenLabel;
            mInitialized = true;
            return {};
        }

        void HsmProvider::Finalize()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mInitialized = false;
            mSlots.clear();
            mKeyData.clear();
            mNextSlotId = 1;
        }

        core::Result<uint32_t> HsmProvider::GenerateKey(
            HsmAlgorithm algorithm, const std::string &label)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mInitialized)
            {
                return core::Result<uint32_t>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            uint32_t slotId = mNextSlotId++;
            HsmSlotInfo info;
            info.SlotId = slotId;
            info.Type = AlgorithmSlotType(algorithm);
            info.Status = HsmSlotStatus::kOccupied;
            info.Label = label;
            info.KeySizeBits = AlgorithmKeySize(algorithm);
            mSlots[slotId] = info;

            // Generate simulated key data (deterministic for testing).
            uint32_t keyBytes = info.KeySizeBits / 8;
            std::vector<uint8_t> key(keyBytes);
            for (uint32_t i = 0; i < keyBytes; ++i)
            {
                key[i] = static_cast<uint8_t>(
                    (slotId * 31 + i * 17) & 0xFF);
            }
            mKeyData[slotId] = std::move(key);

            return slotId;
        }

        core::Result<uint32_t> HsmProvider::ImportKey(
            HsmSlotType type,
            const std::string &label,
            const std::vector<uint8_t> &keyData)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mInitialized)
            {
                return core::Result<uint32_t>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }
            if (keyData.empty())
            {
                return core::Result<uint32_t>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            uint32_t slotId = mNextSlotId++;
            HsmSlotInfo info;
            info.SlotId = slotId;
            info.Type = type;
            info.Status = HsmSlotStatus::kOccupied;
            info.Label = label;
            info.KeySizeBits = static_cast<uint32_t>(keyData.size() * 8);
            mSlots[slotId] = info;
            mKeyData[slotId] = keyData;

            return slotId;
        }

        core::Result<void> HsmProvider::DeleteKey(uint32_t slotId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mInitialized)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }
            auto it = mSlots.find(slotId);
            if (it == mSlots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }
            mSlots.erase(it);
            mKeyData.erase(slotId);
            return {};
        }

        HsmOperationResult HsmProvider::Encrypt(
            uint32_t slotId,
            const std::vector<uint8_t> &plaintext)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            HsmOperationResult result;

            auto keyIt = mKeyData.find(slotId);
            if (keyIt == mKeyData.end() || !mInitialized)
            {
                result.ErrorMessage = "Invalid slot or not initialized";
                return result;
            }

            // Software emulation: XOR with key material (cyclic).
            const auto &key = keyIt->second;
            result.OutputData.resize(plaintext.size());
            for (size_t i = 0; i < plaintext.size(); ++i)
            {
                result.OutputData[i] =
                    plaintext[i] ^ key[i % key.size()];
            }
            result.Success = true;
            return result;
        }

        HsmOperationResult HsmProvider::Decrypt(
            uint32_t slotId,
            const std::vector<uint8_t> &ciphertext)
        {
            // XOR encryption is symmetric, so decrypt == encrypt.
            return Encrypt(slotId, ciphertext);
        }

        HsmOperationResult HsmProvider::Sign(
            uint32_t slotId,
            const std::vector<uint8_t> &data)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            HsmOperationResult result;

            auto keyIt = mKeyData.find(slotId);
            if (keyIt == mKeyData.end() || !mInitialized)
            {
                result.ErrorMessage = "Invalid slot or not initialized";
                return result;
            }

            // Software emulation: HMAC-like hash (XOR-fold with key).
            const auto &key = keyIt->second;
            std::vector<uint8_t> sig(32, 0);
            for (size_t i = 0; i < data.size(); ++i)
            {
                sig[i % 32] ^= data[i];
            }
            for (size_t i = 0; i < 32; ++i)
            {
                sig[i] ^= key[i % key.size()];
            }
            result.OutputData = std::move(sig);
            result.Success = true;
            return result;
        }

        bool HsmProvider::Verify(
            uint32_t slotId,
            const std::vector<uint8_t> &data,
            const std::vector<uint8_t> &signature)
        {
            auto result = Sign(slotId, data);
            if (!result.Success) return false;
            return result.OutputData == signature;
        }

        core::Result<HsmSlotInfo> HsmProvider::GetSlotInfo(
            uint32_t slotId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mSlots.find(slotId);
            if (it == mSlots.end())
            {
                return core::Result<HsmSlotInfo>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }
            return it->second;
        }

        std::vector<HsmSlotInfo> HsmProvider::GetAllSlots() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<HsmSlotInfo> result;
            result.reserve(mSlots.size());
            for (const auto &kv : mSlots)
            {
                result.push_back(kv.second);
            }
            return result;
        }

        bool HsmProvider::IsInitialized() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mInitialized;
        }

        uint32_t HsmProvider::AlgorithmKeySize(HsmAlgorithm alg) const
        {
            switch (alg)
            {
            case HsmAlgorithm::kAes128:     return 128;
            case HsmAlgorithm::kAes256:     return 256;
            case HsmAlgorithm::kRsa2048:    return 2048;
            case HsmAlgorithm::kRsa4096:    return 4096;
            case HsmAlgorithm::kEcdsaP256:  return 256;
            case HsmAlgorithm::kEcdsaP384:  return 384;
            case HsmAlgorithm::kHmacSha256: return 256;
            default:                        return 256;
            }
        }

        HsmSlotType HsmProvider::AlgorithmSlotType(HsmAlgorithm alg) const
        {
            switch (alg)
            {
            case HsmAlgorithm::kAes128:
            case HsmAlgorithm::kAes256:
            case HsmAlgorithm::kHmacSha256:
                return HsmSlotType::kSymmetric;
            case HsmAlgorithm::kRsa2048:
            case HsmAlgorithm::kRsa4096:
            case HsmAlgorithm::kEcdsaP256:
            case HsmAlgorithm::kEcdsaP384:
                return HsmSlotType::kPrivateKey;
            default:
                return HsmSlotType::kSymmetric;
            }
        }
    }
}
