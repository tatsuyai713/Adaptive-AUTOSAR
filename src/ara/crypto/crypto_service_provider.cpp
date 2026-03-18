/// @file src/ara/crypto/crypto_service_provider.cpp
/// @brief Implementation for CryptoServiceProvider (SWS_CRYPT_20100).

#include "./crypto_service_provider.h"
#include <utility>

namespace ara
{
    namespace crypto
    {
        CryptoServiceProvider::CryptoServiceProvider(
            const core::InstanceSpecifier &specifier)
            : mInstanceId{specifier.ToString()}
        {
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::ComputeDigest(
            const std::vector<std::uint8_t> &data,
            DigestAlgorithm algorithm) const
        {
            return ara::crypto::ComputeDigest(data, algorithm);
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::GenerateRandomBytes(
            std::size_t byteCount) const
        {
            return ara::crypto::GenerateRandomBytes(byteCount);
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::ComputeHmac(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &key,
            DigestAlgorithm algorithm) const
        {
            return ara::crypto::ComputeHmac(data, key, algorithm);
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::GenerateSymmetricKey(
            std::size_t keyLengthBytes) const
        {
            return ara::crypto::GenerateSymmetricKey(keyLengthBytes);
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::AesEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv) const
        {
            return ara::crypto::AesEncrypt(plaintext, key, iv);
        }

        core::Result<std::vector<std::uint8_t>>
        CryptoServiceProvider::AesDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv) const
        {
            return ara::crypto::AesDecrypt(ciphertext, key, iv);
        }

        const std::string &
        CryptoServiceProvider::GetInstanceId() const noexcept
        {
            return mInstanceId;
        }

        core::Result<CryptoServiceProvider> LoadCryptoProvider(
            const core::InstanceSpecifier &specifier)
        {
            return core::Result<CryptoServiceProvider>::FromValue(
                CryptoServiceProvider{specifier});
        }
    }
}
