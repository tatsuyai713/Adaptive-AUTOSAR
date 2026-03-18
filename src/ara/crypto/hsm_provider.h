/// @file src/ara/crypto/hsm_provider.h
/// @brief Hardware Security Module (HSM) integration abstraction.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef CRYPTO_HSM_PROVIDER_H
#define CRYPTO_HSM_PROVIDER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief HSM slot type.
        enum class HsmSlotType : uint8_t
        {
            kSymmetric = 0,
            kPrivateKey = 1,
            kPublicKey = 2,
            kCertificate = 3
        };

        /// @brief Status of an HSM slot.
        enum class HsmSlotStatus : uint8_t
        {
            kEmpty = 0,
            kOccupied = 1,
            kLocked = 2
        };

        /// @brief Metadata for one HSM key slot.
        struct HsmSlotInfo
        {
            uint32_t SlotId{0};
            HsmSlotType Type{HsmSlotType::kSymmetric};
            HsmSlotStatus Status{HsmSlotStatus::kEmpty};
            std::string Label;
            uint32_t KeySizeBits{0};
        };

        /// @brief Algorithm identifiers for HSM operations.
        enum class HsmAlgorithm : uint8_t
        {
            kAes128 = 0,
            kAes256 = 1,
            kRsa2048 = 2,
            kRsa4096 = 3,
            kEcdsaP256 = 4,
            kEcdsaP384 = 5,
            kHmacSha256 = 6
        };

        /// @brief Result of an HSM crypto operation.
        struct HsmOperationResult
        {
            bool Success{false};
            std::vector<uint8_t> OutputData;
            std::string ErrorMessage;
        };

        /// @brief Abstraction layer for HSM operations (PKCS#11-style).
        class HsmProvider
        {
        public:
            HsmProvider() = default;
            ~HsmProvider() = default;

            /// @brief Initialize the HSM session.
            core::Result<void> Initialize(const std::string &tokenLabel);

            /// @brief Finalize the HSM session.
            void Finalize();

            /// @brief Generate a key in an HSM slot.
            core::Result<uint32_t> GenerateKey(
                HsmAlgorithm algorithm,
                const std::string &label);

            /// @brief Import key material into an HSM slot.
            core::Result<uint32_t> ImportKey(
                HsmSlotType type,
                const std::string &label,
                const std::vector<uint8_t> &keyData);

            /// @brief Delete a key from an HSM slot.
            core::Result<void> DeleteKey(uint32_t slotId);

            /// @brief Encrypt data using a key in an HSM slot.
            HsmOperationResult Encrypt(
                uint32_t slotId,
                const std::vector<uint8_t> &plaintext);

            /// @brief Decrypt data using a key in an HSM slot.
            HsmOperationResult Decrypt(
                uint32_t slotId,
                const std::vector<uint8_t> &ciphertext);

            /// @brief Sign data using a private key in an HSM slot.
            HsmOperationResult Sign(
                uint32_t slotId,
                const std::vector<uint8_t> &data);

            /// @brief Verify a signature using a public key in an HSM slot.
            bool Verify(
                uint32_t slotId,
                const std::vector<uint8_t> &data,
                const std::vector<uint8_t> &signature);

            /// @brief Get info about a specific slot.
            core::Result<HsmSlotInfo> GetSlotInfo(uint32_t slotId) const;

            /// @brief Get all occupied slots.
            std::vector<HsmSlotInfo> GetAllSlots() const;

            /// @brief Check if the HSM session is active.
            bool IsInitialized() const;

        private:
            mutable std::mutex mMutex;
            bool mInitialized{false};
            std::string mTokenLabel;
            uint32_t mNextSlotId{1};
            std::map<uint32_t, HsmSlotInfo> mSlots;
            /// @brief Simulated key storage for software HSM emulation.
            std::map<uint32_t, std::vector<uint8_t>> mKeyData;

            uint32_t AlgorithmKeySize(HsmAlgorithm alg) const;
            HsmSlotType AlgorithmSlotType(HsmAlgorithm alg) const;
        };
    }
}

#endif
