/// @file src/ara/crypto/hsm_provider.cpp
/// @brief Implementation of HsmProvider (software emulation).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./hsm_provider.h"
#include "./crypto_provider.h"
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

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

            // Generate cryptographically secure random key material.
            uint32_t keyBytes = info.KeySizeBits / 8;
            std::vector<uint8_t> key(keyBytes);
            if (::RAND_bytes(key.data(), static_cast<int>(keyBytes)) != 1)
            {
                // RAND_bytes failed; key slot was already registered, clean up.
                mSlots.erase(slotId);
                return core::Result<uint32_t>::FromError(
                    MakeErrorCode(CryptoErrc::kEntropySourceFailure));
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

            const auto &key = keyIt->second;

            // For AES-128 or AES-256 keys, use real AES-CBC via ara::crypto.
            // A fresh 16-byte IV is prepended to the ciphertext output.
            if (key.size() == 16 || key.size() == 32)
            {
                std::vector<uint8_t> iv(16);
                if (::RAND_bytes(iv.data(), 16) != 1)
                {
                    result.ErrorMessage = "IV generation failed";
                    return result;
                }
                auto encResult = ara::crypto::AesEncrypt(plaintext, key, iv);
                if (!encResult.HasValue())
                {
                    result.ErrorMessage = "AES encrypt failed";
                    return result;
                }
                // Output = IV (16 bytes) || ciphertext
                result.OutputData = iv;
                const auto &ct = encResult.Value();
                result.OutputData.insert(
                    result.OutputData.end(), ct.begin(), ct.end());
                result.Success = true;
                return result;
            }

            // Fallback for non-AES key sizes: use HMAC-based XOR stream cipher.
            std::vector<uint8_t> keystream;
            keystream.reserve(plaintext.size());
            for (size_t block = 0; keystream.size() < plaintext.size(); ++block)
            {
                std::vector<uint8_t> counter{
                    static_cast<uint8_t>(block >> 24),
                    static_cast<uint8_t>(block >> 16),
                    static_cast<uint8_t>(block >> 8),
                    static_cast<uint8_t>(block)};
                auto hmacResult = ara::crypto::ComputeHmac(counter, key);
                if (!hmacResult.HasValue()) break;
                for (auto b : hmacResult.Value())
                {
                    keystream.push_back(b);
                    if (keystream.size() == plaintext.size()) break;
                }
            }
            result.OutputData.resize(plaintext.size());
            for (size_t i = 0; i < plaintext.size(); ++i)
            {
                result.OutputData[i] = plaintext[i] ^ keystream[i];
            }
            result.Success = true;
            return result;
        }

        HsmOperationResult HsmProvider::Decrypt(
            uint32_t slotId,
            const std::vector<uint8_t> &ciphertext)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            HsmOperationResult result;

            auto keyIt = mKeyData.find(slotId);
            if (keyIt == mKeyData.end() || !mInitialized)
            {
                result.ErrorMessage = "Invalid slot or not initialized";
                return result;
            }

            const auto &key = keyIt->second;

            // For AES keys, extract the IV (first 16 bytes) and decrypt the rest.
            if ((key.size() == 16 || key.size() == 32) &&
                ciphertext.size() > 16)
            {
                std::vector<uint8_t> iv(
                    ciphertext.begin(), ciphertext.begin() + 16);
                std::vector<uint8_t> data(
                    ciphertext.begin() + 16, ciphertext.end());
                auto decResult = ara::crypto::AesDecrypt(data, key, iv);
                if (!decResult.HasValue())
                {
                    result.ErrorMessage = "AES decrypt failed";
                    return result;
                }
                result.OutputData = decResult.Value();
                result.Success = true;
                return result;
            }

            // Fallback: XOR stream (symmetric).
            std::vector<uint8_t> keystream;
            keystream.reserve(ciphertext.size());
            for (size_t block = 0; keystream.size() < ciphertext.size(); ++block)
            {
                std::vector<uint8_t> counter{
                    static_cast<uint8_t>(block >> 24),
                    static_cast<uint8_t>(block >> 16),
                    static_cast<uint8_t>(block >> 8),
                    static_cast<uint8_t>(block)};
                auto hmacResult = ara::crypto::ComputeHmac(counter, key);
                if (!hmacResult.HasValue()) break;
                for (auto b : hmacResult.Value())
                {
                    keystream.push_back(b);
                    if (keystream.size() == ciphertext.size()) break;
                }
            }
            result.OutputData.resize(ciphertext.size());
            for (size_t i = 0; i < ciphertext.size(); ++i)
            {
                result.OutputData[i] = ciphertext[i] ^ keystream[i];
            }
            result.Success = true;
            return result;
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

            const auto &key = keyIt->second;

            // Use real HMAC-SHA256 via ara::crypto.
            auto hmacResult = ara::crypto::ComputeHmac(data, key);
            if (!hmacResult.HasValue())
            {
                result.ErrorMessage = "HMAC-SHA256 computation failed";
                return result;
            }
            result.OutputData = hmacResult.Value();
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
