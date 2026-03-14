/// @file src/ara/crypto/crypto_provider.cpp
/// @brief Implementation for crypto provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./crypto_provider.h"

#include <cstring>
#include <limits>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#endif

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

            const EVP_CIPHER *ResolveAesGcmCipher(std::size_t keyLength)
            {
                if (keyLength == 16U)
                {
                    return EVP_aes_128_gcm();
                }
                else if (keyLength == 32U)
                {
                    return EVP_aes_256_gcm();
                }
                else
                {
                    return nullptr;
                }
            }

            const std::size_t cAesGcmTagSize{16U};
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

        core::Result<AesGcmResult> AesGcmEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv,
            const std::vector<std::uint8_t> &aad)
        {
            const EVP_CIPHER *_cipher{ResolveAesGcmCipher(key.size())};
            if (_cipher == nullptr)
            {
                return core::Result<AesGcmResult>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            if (iv.empty())
            {
                return core::Result<AesGcmResult>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_CIPHER_CTX *_context{EVP_CIPHER_CTX_new()};
            if (_context == nullptr)
            {
                return core::Result<AesGcmResult>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::vector<std::uint8_t> _ciphertext(plaintext.size());
            std::vector<std::uint8_t> _tag(cAesGcmTagSize);
            int _outLength{0};

            bool _success{true};
            _success = _success &&
                       (EVP_EncryptInit_ex(_context, _cipher, nullptr, nullptr, nullptr) == 1);
            _success = _success &&
                       (EVP_CIPHER_CTX_ctrl(_context, EVP_CTRL_GCM_SET_IVLEN,
                                            static_cast<int>(iv.size()), nullptr) == 1);
            _success = _success &&
                       (EVP_EncryptInit_ex(_context, nullptr, nullptr,
                                           key.data(), iv.data()) == 1);

            if (_success && !aad.empty())
            {
                _success = _success &&
                           (EVP_EncryptUpdate(_context, nullptr, &_outLength,
                                              aad.data(), static_cast<int>(aad.size())) == 1);
            }

            if (_success && !plaintext.empty())
            {
                _success = _success &&
                           (EVP_EncryptUpdate(_context, _ciphertext.data(), &_outLength,
                                              plaintext.data(),
                                              static_cast<int>(plaintext.size())) == 1);
            }

            if (_success)
            {
                _success = _success &&
                           (EVP_EncryptFinal_ex(_context, _ciphertext.data() + _outLength,
                                                &_outLength) == 1);
            }

            if (_success)
            {
                _success = _success &&
                           (EVP_CIPHER_CTX_ctrl(_context, EVP_CTRL_GCM_GET_TAG,
                                                static_cast<int>(cAesGcmTagSize),
                                                _tag.data()) == 1);
            }

            EVP_CIPHER_CTX_free(_context);

            if (!_success)
            {
                return core::Result<AesGcmResult>::FromError(
                    MakeErrorCode(CryptoErrc::kEncryptionFailure));
            }

            AesGcmResult _result;
            _result.Ciphertext = std::move(_ciphertext);
            _result.Tag = std::move(_tag);
            return core::Result<AesGcmResult>::FromValue(std::move(_result));
        }

        core::Result<std::vector<std::uint8_t>> AesGcmDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv,
            const std::vector<std::uint8_t> &tag,
            const std::vector<std::uint8_t> &aad)
        {
            const EVP_CIPHER *_cipher{ResolveAesGcmCipher(key.size())};
            if (_cipher == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            if (iv.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (tag.size() != cAesGcmTagSize)
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

            std::vector<std::uint8_t> _plaintext(ciphertext.size());
            int _outLength{0};
            int _totalLength{0};

            bool _success{true};
            _success = _success &&
                       (EVP_DecryptInit_ex(_context, _cipher, nullptr, nullptr, nullptr) == 1);
            _success = _success &&
                       (EVP_CIPHER_CTX_ctrl(_context, EVP_CTRL_GCM_SET_IVLEN,
                                            static_cast<int>(iv.size()), nullptr) == 1);
            _success = _success &&
                       (EVP_DecryptInit_ex(_context, nullptr, nullptr,
                                           key.data(), iv.data()) == 1);

            if (_success && !aad.empty())
            {
                _success = _success &&
                           (EVP_DecryptUpdate(_context, nullptr, &_outLength,
                                              aad.data(), static_cast<int>(aad.size())) == 1);
            }

            if (_success && !ciphertext.empty())
            {
                _success = _success &&
                           (EVP_DecryptUpdate(_context, _plaintext.data(), &_outLength,
                                              ciphertext.data(),
                                              static_cast<int>(ciphertext.size())) == 1);
                _totalLength = _outLength;
            }

            // Set expected tag before final
            if (_success)
            {
                std::vector<std::uint8_t> _tagCopy(tag);
                _success = _success &&
                           (EVP_CIPHER_CTX_ctrl(_context, EVP_CTRL_GCM_SET_TAG,
                                                static_cast<int>(cAesGcmTagSize),
                                                _tagCopy.data()) == 1);
            }

            if (_success)
            {
                const int _finalResult = EVP_DecryptFinal_ex(
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

        core::Result<std::vector<std::uint8_t>> DeriveKeyPbkdf2(
            const std::vector<std::uint8_t> &password,
            const std::vector<std::uint8_t> &salt,
            std::uint32_t iterations,
            std::size_t keyLength,
            DigestAlgorithm algorithm)
        {
            if (salt.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (iterations == 0U)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (keyLength == 0U)
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

            std::vector<std::uint8_t> _derivedKey(keyLength);

            const int _result = PKCS5_PBKDF2_HMAC(
                reinterpret_cast<const char *>(password.empty() ? nullptr : password.data()),
                static_cast<int>(password.size()),
                salt.data(),
                static_cast<int>(salt.size()),
                static_cast<int>(iterations),
                _md,
                static_cast<int>(keyLength),
                _derivedKey.data());

            if (_result != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_derivedKey));
        }

        core::Result<std::vector<std::uint8_t>> DeriveKeyHkdf(
            const std::vector<std::uint8_t> &inputKey,
            const std::vector<std::uint8_t> &salt,
            const std::vector<std::uint8_t> &info,
            std::size_t keyLength,
            DigestAlgorithm algorithm)
        {
            if (inputKey.empty())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            if (keyLength == 0U)
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

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            // OpenSSL 3.0+ EVP_KDF API
            EVP_KDF *_kdf{EVP_KDF_fetch(nullptr, "HKDF", nullptr)};
            if (_kdf == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            EVP_KDF_CTX *_kctx{EVP_KDF_CTX_new(_kdf)};
            EVP_KDF_free(_kdf);
            if (_kctx == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            const char *_digestName{EVP_MD_get0_name(_md)};
            OSSL_PARAM _params[5];
            int _paramIdx{0};
            _params[_paramIdx++] = OSSL_PARAM_construct_utf8_string(
                OSSL_KDF_PARAM_DIGEST,
                const_cast<char *>(_digestName), 0);
            _params[_paramIdx++] = OSSL_PARAM_construct_octet_string(
                OSSL_KDF_PARAM_KEY,
                const_cast<std::uint8_t *>(inputKey.data()),
                inputKey.size());
            if (!salt.empty())
            {
                _params[_paramIdx++] = OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_SALT,
                    const_cast<std::uint8_t *>(salt.data()),
                    salt.size());
            }
            if (!info.empty())
            {
                _params[_paramIdx++] = OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_INFO,
                    const_cast<std::uint8_t *>(info.data()),
                    info.size());
            }
            _params[_paramIdx] = OSSL_PARAM_construct_end();

            std::vector<std::uint8_t> _derivedKey(keyLength);
            bool _success{EVP_KDF_derive(
                _kctx, _derivedKey.data(), keyLength, _params) == 1};

            EVP_KDF_CTX_free(_kctx);
#else
            // OpenSSL 1.x EVP_PKEY HKDF API
            EVP_PKEY_CTX *_context{EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr)};
            if (_context == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool _success{true};
            _success = _success && (EVP_PKEY_derive_init(_context) == 1);
            _success = _success &&
                       (EVP_PKEY_CTX_set_hkdf_md(_context, _md) == 1);
            _success = _success &&
                       (EVP_PKEY_CTX_set1_hkdf_key(
                           _context,
                           inputKey.data(),
                           static_cast<int>(inputKey.size())) == 1);

            if (_success)
            {
                if (!salt.empty())
                {
                    _success = _success &&
                               (EVP_PKEY_CTX_set1_hkdf_salt(
                                   _context,
                                   salt.data(),
                                   static_cast<int>(salt.size())) == 1);
                }
            }

            if (_success && !info.empty())
            {
                _success = _success &&
                           (EVP_PKEY_CTX_add1_hkdf_info(
                               _context,
                               info.data(),
                               static_cast<int>(info.size())) == 1);
            }

            std::vector<std::uint8_t> _derivedKey(keyLength);

            if (_success)
            {
                std::size_t _outLen{keyLength};
                _success = _success &&
                           (EVP_PKEY_derive(_context, _derivedKey.data(), &_outLen) == 1);
            }

            EVP_PKEY_CTX_free(_context);
#endif

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_derivedKey));
        }

        namespace
        {
            /// @brief Common helper for AES-CTR encrypt/decrypt (same operation in CTR mode).
            core::Result<std::vector<std::uint8_t>> AesCtrProcess(
                const std::vector<std::uint8_t> &input,
                const std::vector<std::uint8_t> &key,
                const std::vector<std::uint8_t> &iv)
            {
                const EVP_CIPHER *_cipher{nullptr};
                if (key.size() == 16U)
                {
                    _cipher = EVP_aes_128_ctr();
                }
                else if (key.size() == 32U)
                {
                    _cipher = EVP_aes_256_ctr();
                }

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

                // CTR mode: output size equals input size (stream cipher, no padding)
                std::vector<std::uint8_t> _output(input.empty() ? 0U : input.size());
                int _outLength{0};
                int _totalLength{0};

                bool _success{true};
                _success = _success &&
                           (EVP_EncryptInit_ex(_context, _cipher, nullptr,
                                               key.data(), iv.data()) == 1);

                if (_success && !input.empty())
                {
                    _success = _success &&
                               (EVP_EncryptUpdate(_context, _output.data(), &_outLength,
                                                  input.data(),
                                                  static_cast<int>(input.size())) == 1);
                    _totalLength = _outLength;
                }

                if (_success)
                {
                    // For CTR (stream) mode, EncryptFinal_ex produces 0 extra bytes
                    std::vector<std::uint8_t> _finalBuf(cAesBlockSize);
                    _success = _success &&
                               (EVP_EncryptFinal_ex(_context, _finalBuf.data(), &_outLength) == 1);
                    _totalLength += _outLength;
                }

                EVP_CIPHER_CTX_free(_context);

                if (!_success)
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(CryptoErrc::kEncryptionFailure));
                }

                _output.resize(static_cast<std::size_t>(_totalLength));
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(_output));
            }

            /// @brief Common helper for ChaCha20 encrypt/decrypt (same operation).
            core::Result<std::vector<std::uint8_t>> ChaCha20Process(
                const std::vector<std::uint8_t> &input,
                const std::vector<std::uint8_t> &key,
                const std::vector<std::uint8_t> &iv)
            {
                // OpenSSL EVP_chacha20: key=32 bytes, iv=16 bytes
                // (4-byte counter LE + 12-byte nonce, per OpenSSL convention)
                static constexpr std::size_t cChaCha20KeySize{32U};
                static constexpr std::size_t cChaCha20IvSize{16U};

                if (key.size() != cChaCha20KeySize)
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(CryptoErrc::kInvalidKeySize));
                }

                if (iv.size() != cChaCha20IvSize)
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

                std::vector<std::uint8_t> _output(input.empty() ? 0U : input.size());
                int _outLength{0};
                int _totalLength{0};

                bool _success{true};
                _success = _success &&
                           (EVP_EncryptInit_ex(_context, EVP_chacha20(), nullptr,
                                               key.data(), iv.data()) == 1);

                if (_success && !input.empty())
                {
                    _success = _success &&
                               (EVP_EncryptUpdate(_context, _output.data(), &_outLength,
                                                  input.data(),
                                                  static_cast<int>(input.size())) == 1);
                    _totalLength = _outLength;
                }

                if (_success)
                {
                    std::vector<std::uint8_t> _finalBuf(64U); // ChaCha20 block size
                    _success = _success &&
                               (EVP_EncryptFinal_ex(_context, _finalBuf.data(), &_outLength) == 1);
                    _totalLength += _outLength;
                }

                EVP_CIPHER_CTX_free(_context);

                if (!_success)
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(CryptoErrc::kEncryptionFailure));
                }

                _output.resize(static_cast<std::size_t>(_totalLength));
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(_output));
            }
        } // anonymous namespace

        core::Result<std::vector<std::uint8_t>> AesCtrEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            return AesCtrProcess(plaintext, key, iv);
        }

        core::Result<std::vector<std::uint8_t>> AesCtrDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            // AES-CTR: decryption is identical to encryption
            return AesCtrProcess(ciphertext, key, iv);
        }

        core::Result<std::vector<std::uint8_t>> ChaCha20Encrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            return ChaCha20Process(plaintext, key, iv);
        }

        core::Result<std::vector<std::uint8_t>> ChaCha20Decrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            // ChaCha20: decryption is identical to encryption
            return ChaCha20Process(ciphertext, key, iv);
        }
    }
}
