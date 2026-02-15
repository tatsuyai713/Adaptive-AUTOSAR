/// @file src/ara/crypto/crypto_provider.cpp
/// @brief Implementation for crypto provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./crypto_provider.h"

#include <limits>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            const std::size_t cAesBlockSize{16U};

            const EVP_MD *ResolveDigestAlgorithm(DigestAlgorithm algorithm)
            {
                switch (algorithm)
                {
                case DigestAlgorithm::kSha1:
                    return EVP_sha1();
                case DigestAlgorithm::kSha256:
                    return EVP_sha256();
                case DigestAlgorithm::kSha384:
                    return EVP_sha384();
                case DigestAlgorithm::kSha512:
                    return EVP_sha512();
                default:
                    return nullptr;
                }
            }

            const EVP_CIPHER *ResolveAesCipher(std::size_t keyLength)
            {
                if (keyLength == 16U)
                {
                    return EVP_aes_128_cbc();
                }
                else if (keyLength == 32U)
                {
                    return EVP_aes_256_cbc();
                }
                else
                {
                    return nullptr;
                }
            }
        }

        core::Result<std::vector<std::uint8_t>> ComputeDigest(
            const std::vector<std::uint8_t> &data,
            DigestAlgorithm algorithm)
        {
            const EVP_MD *_md{ResolveDigestAlgorithm(algorithm)};
            if (_md == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kUnsupportedAlgorithm));
            }

            EVP_MD_CTX *_context{EVP_MD_CTX_new()};
            if (_context == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::vector<std::uint8_t> _digest(static_cast<std::size_t>(EVP_MD_size(_md)));
            unsigned int _digestLength{0U};

            bool _success{true};
            _success = _success && (EVP_DigestInit_ex(_context, _md, nullptr) == 1);
            if (!data.empty())
            {
                _success = _success &&
                           (EVP_DigestUpdate(_context, data.data(), data.size()) == 1);
            }
            _success = _success &&
                       (EVP_DigestFinal_ex(_context, _digest.data(), &_digestLength) == 1);

            EVP_MD_CTX_free(_context);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            _digest.resize(static_cast<std::size_t>(_digestLength));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_digest));
        }

        core::Result<std::vector<std::uint8_t>> GenerateRandomBytes(
            std::size_t byteCount)
        {
            if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            std::vector<std::uint8_t> _bytes(byteCount, 0U);
            if (byteCount == 0U)
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(_bytes));
            }

            const int _result = RAND_bytes(
                _bytes.data(),
                static_cast<int>(byteCount));
            if (_result != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kEntropySourceFailure));
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_bytes));
        }

        core::Result<std::vector<std::uint8_t>> ComputeHmac(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &key,
            DigestAlgorithm algorithm)
        {
            if (key.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            const EVP_MD *_md{ResolveDigestAlgorithm(algorithm)};
            if (_md == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kUnsupportedAlgorithm));
            }

            std::vector<std::uint8_t> _hmac(static_cast<std::size_t>(EVP_MD_size(_md)));
            unsigned int _hmacLength{0U};

            unsigned char *_hmacResult = HMAC(
                _md,
                key.data(),
                static_cast<int>(key.size()),
                data.empty() ? nullptr : data.data(),
                data.size(),
                _hmac.data(),
                &_hmacLength);

            if (_hmacResult == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            _hmac.resize(static_cast<std::size_t>(_hmacLength));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_hmac));
        }

        core::Result<std::vector<std::uint8_t>> GenerateSymmetricKey(
            std::size_t keyLengthBytes)
        {
            if (keyLengthBytes != 16U && keyLengthBytes != 32U)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            return GenerateRandomBytes(keyLengthBytes);
        }

        core::Result<std::vector<std::uint8_t>> AesEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            const EVP_CIPHER *_cipher{ResolveAesCipher(key.size())};
            if (_cipher == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            if (iv.size() != cAesBlockSize)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_CIPHER_CTX *_context{EVP_CIPHER_CTX_new()};
            if (_context == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            // Output buffer: plaintext size + one block for potential padding
            std::vector<std::uint8_t> _ciphertext(plaintext.size() + cAesBlockSize);
            int _outLength{0};
            int _totalLength{0};

            bool _success{true};
            _success = _success &&
                       (EVP_EncryptInit_ex(_context, _cipher, nullptr, key.data(), iv.data()) == 1);

            if (_success && !plaintext.empty())
            {
                _success = _success &&
                           (EVP_EncryptUpdate(_context, _ciphertext.data(), &_outLength,
                                              plaintext.data(), static_cast<int>(plaintext.size())) == 1);
                _totalLength = _outLength;
            }

            if (_success)
            {
                _success = _success &&
                           (EVP_EncryptFinal_ex(_context, _ciphertext.data() + _totalLength, &_outLength) == 1);
                _totalLength += _outLength;
            }

            EVP_CIPHER_CTX_free(_context);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kEncryptionFailure));
            }

            _ciphertext.resize(static_cast<std::size_t>(_totalLength));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_ciphertext));
        }

        core::Result<std::vector<std::uint8_t>> AesDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            const EVP_CIPHER *_cipher{ResolveAesCipher(key.size())};
            if (_cipher == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            if (iv.size() != cAesBlockSize)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (ciphertext.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_CIPHER_CTX *_context{EVP_CIPHER_CTX_new()};
            if (_context == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::vector<std::uint8_t> _plaintext(ciphertext.size() + cAesBlockSize);
            int _outLength{0};
            int _totalLength{0};

            bool _success{true};
            _success = _success &&
                       (EVP_DecryptInit_ex(_context, _cipher, nullptr, key.data(), iv.data()) == 1);

            if (_success)
            {
                _success = _success &&
                           (EVP_DecryptUpdate(_context, _plaintext.data(), &_outLength,
                                              ciphertext.data(), static_cast<int>(ciphertext.size())) == 1);
                _totalLength = _outLength;
            }

            if (_success)
            {
                int _finalResult = EVP_DecryptFinal_ex(
                    _context, _plaintext.data() + _totalLength, &_outLength);
                if (_finalResult != 1)
                {
                    EVP_CIPHER_CTX_free(_context);
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(CryptoErrc::kDecryptionFailure));
                }
                _totalLength += _outLength;
            }

            EVP_CIPHER_CTX_free(_context);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kDecryptionFailure));
            }

            _plaintext.resize(static_cast<std::size_t>(_totalLength));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_plaintext));
        }
    }
}
