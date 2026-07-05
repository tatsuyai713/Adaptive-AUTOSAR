/// @file src/ara/crypto/hsm_provider.cpp
/// @brief Implementation of HsmProvider (software emulation).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./hsm_provider.h"
#include "./crypto_provider.h"
#include "./ecdsa_provider.h"
#include "./rsa_provider.h"
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
            mAlgorithms.clear();
            mKeyData.clear();
            mPublicKeyData.clear();
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

            std::vector<uint8_t> key;
            std::vector<uint8_t> publicKey;
            switch (algorithm)
            {
            case HsmAlgorithm::kRsa2048:
            case HsmAlgorithm::kRsa4096:
            {
                auto keyPair = GenerateRsaKeyPair(AlgorithmKeySize(algorithm));
                if (!keyPair.HasValue())
                {
                    return core::Result<uint32_t>::FromError(keyPair.Error());
                }
                key = keyPair.Value().PrivateKeyDer;
                publicKey = keyPair.Value().PublicKeyDer;
                break;
            }
            case HsmAlgorithm::kEcdsaP256:
            case HsmAlgorithm::kEcdsaP384:
            {
                auto curve = (algorithm == HsmAlgorithm::kEcdsaP256)
                                 ? EllipticCurve::kP256
                                 : EllipticCurve::kP384;
                auto keyPair = GenerateEcKeyPair(curve);
                if (!keyPair.HasValue())
                {
                    return core::Result<uint32_t>::FromError(keyPair.Error());
                }
                key = keyPair.Value().PrivateKeyDer;
                publicKey = keyPair.Value().PublicKeyDer;
                break;
            }
            default:
            {
                uint32_t keyBytes = AlgorithmKeySize(algorithm) / 8;
                key.resize(keyBytes);
                if (::RAND_bytes(key.data(), static_cast<int>(keyBytes)) != 1)
                {
                    return core::Result<uint32_t>::FromError(
                        MakeErrorCode(CryptoErrc::kEntropySourceFailure));
                }
                break;
            }
            }

            uint32_t slotId = mNextSlotId++;
            HsmSlotInfo info;
            info.SlotId = slotId;
            info.Type = AlgorithmSlotType(algorithm);
            info.Status = HsmSlotStatus::kOccupied;
            info.Label = label;
            info.KeySizeBits = AlgorithmKeySize(algorithm);
            mSlots[slotId] = info;

            mAlgorithms[slotId] = algorithm;
            mKeyData[slotId] = std::move(key);
            if (!publicKey.empty())
            {
                mPublicKeyData[slotId] = std::move(publicKey);
            }

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
            if (type == HsmSlotType::kSymmetric &&
                (keyData.size() == 16U || keyData.size() == 32U))
            {
                mAlgorithms[slotId] = (keyData.size() == 16U)
                                          ? HsmAlgorithm::kAes128
                                          : HsmAlgorithm::kAes256;
            }
            else if (type == HsmSlotType::kSymmetric)
            {
                mAlgorithms[slotId] = HsmAlgorithm::kHmacSha256;
            }
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
            mAlgorithms.erase(slotId);
            mKeyData.erase(slotId);
            mPublicKeyData.erase(slotId);
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
            auto algIt = mAlgorithms.find(slotId);
            HsmAlgorithm algorithm = (algIt != mAlgorithms.end())
                                         ? algIt->second
                                         : HsmAlgorithm::kHmacSha256;

            if (algorithm == HsmAlgorithm::kRsa2048 ||
                algorithm == HsmAlgorithm::kRsa4096)
            {
                auto publicIt = mPublicKeyData.find(slotId);
                if (publicIt == mPublicKeyData.end())
                {
                    result.ErrorMessage = "RSA public key is unavailable";
                    return result;
                }
                auto encResult = ara::crypto::RsaEncrypt(plaintext, publicIt->second);
                if (!encResult.HasValue())
                {
                    result.ErrorMessage = "RSA encrypt failed";
                    return result;
                }
                result.OutputData = encResult.Value();
                result.Success = true;
                return result;
            }

            // For AES-128 or AES-256 keys, use real AES-CBC via ara::crypto.
            // A fresh 16-byte IV is prepended to the ciphertext output.
            if (algorithm == HsmAlgorithm::kAes128 ||
                algorithm == HsmAlgorithm::kAes256)
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

            result.ErrorMessage = "Algorithm does not support encryption";
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
            auto algIt = mAlgorithms.find(slotId);
            HsmAlgorithm algorithm = (algIt != mAlgorithms.end())
                                         ? algIt->second
                                         : HsmAlgorithm::kHmacSha256;

            if (algorithm == HsmAlgorithm::kRsa2048 ||
                algorithm == HsmAlgorithm::kRsa4096)
            {
                auto decResult = ara::crypto::RsaDecrypt(ciphertext, key);
                if (!decResult.HasValue())
                {
                    result.ErrorMessage = "RSA decrypt failed";
                    return result;
                }
                result.OutputData = decResult.Value();
                result.Success = true;
                return result;
            }

            // For AES keys, extract the IV (first 16 bytes) and decrypt the rest.
            if ((algorithm == HsmAlgorithm::kAes128 ||
                 algorithm == HsmAlgorithm::kAes256) &&
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

            result.ErrorMessage = "Algorithm does not support decryption";
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
            auto algIt = mAlgorithms.find(slotId);
            HsmAlgorithm algorithm = (algIt != mAlgorithms.end())
                                         ? algIt->second
                                         : HsmAlgorithm::kHmacSha256;

            if (algorithm == HsmAlgorithm::kRsa2048 ||
                algorithm == HsmAlgorithm::kRsa4096)
            {
                auto signResult = ara::crypto::RsaSign(data, key);
                if (!signResult.HasValue())
                {
                    result.ErrorMessage = "RSA sign failed";
                    return result;
                }
                result.OutputData = signResult.Value();
                result.Success = true;
                return result;
            }

            if (algorithm == HsmAlgorithm::kEcdsaP256 ||
                algorithm == HsmAlgorithm::kEcdsaP384)
            {
                auto signResult = ara::crypto::EcdsaSign(data, key);
                if (!signResult.HasValue())
                {
                    result.ErrorMessage = "ECDSA sign failed";
                    return result;
                }
                result.OutputData = signResult.Value();
                result.Success = true;
                return result;
            }

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
            std::lock_guard<std::mutex> lock(mMutex);
            auto algIt = mAlgorithms.find(slotId);
            HsmAlgorithm algorithm = (algIt != mAlgorithms.end())
                                         ? algIt->second
                                         : HsmAlgorithm::kHmacSha256;

            if (algorithm == HsmAlgorithm::kRsa2048 ||
                algorithm == HsmAlgorithm::kRsa4096)
            {
                auto publicIt = mPublicKeyData.find(slotId);
                if (publicIt == mPublicKeyData.end()) return false;
                auto verifyResult = ara::crypto::RsaVerify(
                    data, signature, publicIt->second);
                return verifyResult.HasValue() && verifyResult.Value();
            }

            if (algorithm == HsmAlgorithm::kEcdsaP256 ||
                algorithm == HsmAlgorithm::kEcdsaP384)
            {
                auto publicIt = mPublicKeyData.find(slotId);
                if (publicIt == mPublicKeyData.end()) return false;
                auto verifyResult = ara::crypto::EcdsaVerify(
                    data, signature, publicIt->second);
                return verifyResult.HasValue() && verifyResult.Value();
            }

            auto keyIt = mKeyData.find(slotId);
            if (keyIt == mKeyData.end() || !mInitialized) return false;
            auto hmacResult = ara::crypto::ComputeHmac(data, keyIt->second);
            return hmacResult.HasValue() && hmacResult.Value() == signature;
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
