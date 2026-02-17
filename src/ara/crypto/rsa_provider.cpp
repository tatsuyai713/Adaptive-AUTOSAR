/// @file src/ara/crypto/rsa_provider.cpp
/// @brief Implementation for RSA asymmetric cryptography provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./rsa_provider.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            const EVP_MD *ResolveDigest(DigestAlgorithm algorithm)
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
        }

        core::Result<RsaKeyPair> GenerateRsaKeyPair(std::uint32_t keySizeBits)
        {
            if (keySizeBits != 2048U && keySizeBits != 4096U)
            {
                return core::Result<RsaKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeySize));
            }

            EVP_PKEY_CTX *_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
            if (_ctx == nullptr)
            {
                return core::Result<RsaKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
            }

            EVP_PKEY *_pkey = nullptr;
            bool _success = true;
            _success = _success && (EVP_PKEY_keygen_init(_ctx) > 0);
            _success = _success && (EVP_PKEY_CTX_set_rsa_keygen_bits(
                                        _ctx, static_cast<int>(keySizeBits)) > 0);
            _success = _success && (EVP_PKEY_keygen(_ctx, &_pkey) > 0);
            EVP_PKEY_CTX_free(_ctx);

            if (!_success || _pkey == nullptr)
            {
                if (_pkey != nullptr)
                {
                    EVP_PKEY_free(_pkey);
                }
                return core::Result<RsaKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
            }

            RsaKeyPair _result;

            // Extract public key DER
            {
                unsigned char *_buf = nullptr;
                int _len = i2d_PUBKEY(_pkey, &_buf);
                if (_len > 0 && _buf != nullptr)
                {
                    _result.PublicKeyDer.assign(_buf, _buf + _len);
                    OPENSSL_free(_buf);
                }
                else
                {
                    EVP_PKEY_free(_pkey);
                    return core::Result<RsaKeyPair>::FromError(
                        MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
                }
            }

            // Extract private key DER
            {
                unsigned char *_buf = nullptr;
                int _len = i2d_PrivateKey(_pkey, &_buf);
                if (_len > 0 && _buf != nullptr)
                {
                    _result.PrivateKeyDer.assign(_buf, _buf + _len);
                    OPENSSL_free(_buf);
                }
                else
                {
                    EVP_PKEY_free(_pkey);
                    return core::Result<RsaKeyPair>::FromError(
                        MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
                }
            }

            EVP_PKEY_free(_pkey);
            return core::Result<RsaKeyPair>::FromValue(std::move(_result));
        }

        core::Result<std::vector<std::uint8_t>> RsaSign(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &privateKeyDer,
            DigestAlgorithm algorithm)
        {
            const EVP_MD *_md = ResolveDigest(algorithm);
            if (_md == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kUnsupportedAlgorithm));
            }

            const unsigned char *_derPtr = privateKeyDer.data();
            EVP_PKEY *_pkey = d2i_AutoPrivateKey(
                nullptr, &_derPtr, static_cast<long>(privateKeyDer.size()));
            if (_pkey == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));
            }

            EVP_MD_CTX *_mdCtx = EVP_MD_CTX_new();
            if (_mdCtx == nullptr)
            {
                EVP_PKEY_free(_pkey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool _success = true;
            _success = _success &&
                       (EVP_DigestSignInit(_mdCtx, nullptr, _md, nullptr, _pkey) == 1);
            _success = _success &&
                       (EVP_DigestSignUpdate(_mdCtx, data.data(), data.size()) == 1);

            std::size_t _sigLen{0U};
            _success = _success &&
                       (EVP_DigestSignFinal(_mdCtx, nullptr, &_sigLen) == 1);

            std::vector<std::uint8_t> _signature(_sigLen);
            _success = _success &&
                       (EVP_DigestSignFinal(_mdCtx, _signature.data(), &_sigLen) == 1);

            EVP_MD_CTX_free(_mdCtx);
            EVP_PKEY_free(_pkey);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kSignatureFailure));
            }

            _signature.resize(_sigLen);
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_signature));
        }

        core::Result<bool> RsaVerify(
            const std::vector<std::uint8_t> &data,
            const std::vector<std::uint8_t> &signature,
            const std::vector<std::uint8_t> &publicKeyDer,
            DigestAlgorithm algorithm)
        {
            const EVP_MD *_md = ResolveDigest(algorithm);
            if (_md == nullptr)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kUnsupportedAlgorithm));
            }

            const unsigned char *_derPtr = publicKeyDer.data();
            EVP_PKEY *_pkey = d2i_PUBKEY(
                nullptr, &_derPtr, static_cast<long>(publicKeyDer.size()));
            if (_pkey == nullptr)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));
            }

            EVP_MD_CTX *_mdCtx = EVP_MD_CTX_new();
            if (_mdCtx == nullptr)
            {
                EVP_PKEY_free(_pkey);
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool _initOk = (EVP_DigestVerifyInit(
                                 _mdCtx, nullptr, _md, nullptr, _pkey) == 1);
            bool _updateOk = _initOk &&
                             (EVP_DigestVerifyUpdate(
                                  _mdCtx, data.data(), data.size()) == 1);

            int _verifyResult = 0;
            if (_updateOk)
            {
                _verifyResult = EVP_DigestVerifyFinal(
                    _mdCtx, signature.data(), signature.size());
            }

            EVP_MD_CTX_free(_mdCtx);
            EVP_PKEY_free(_pkey);

            if (!_initOk || !_updateOk)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kVerificationFailure));
            }

            return core::Result<bool>::FromValue(_verifyResult == 1);
        }

        core::Result<std::vector<std::uint8_t>> RsaEncrypt(
            const std::vector<std::uint8_t> &plaintext,
            const std::vector<std::uint8_t> &publicKeyDer)
        {
            const unsigned char *_derPtr = publicKeyDer.data();
            EVP_PKEY *_pkey = d2i_PUBKEY(
                nullptr, &_derPtr, static_cast<long>(publicKeyDer.size()));
            if (_pkey == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));
            }

            EVP_PKEY_CTX *_ctx = EVP_PKEY_CTX_new(_pkey, nullptr);
            if (_ctx == nullptr)
            {
                EVP_PKEY_free(_pkey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool _success = true;
            _success = _success && (EVP_PKEY_encrypt_init(_ctx) > 0);
            _success = _success && (EVP_PKEY_CTX_set_rsa_padding(
                                        _ctx, RSA_PKCS1_OAEP_PADDING) > 0);

            std::size_t _outLen{0U};
            _success = _success &&
                       (EVP_PKEY_encrypt(_ctx, nullptr, &_outLen,
                                         plaintext.data(), plaintext.size()) > 0);

            std::vector<std::uint8_t> _ciphertext(_outLen);
            _success = _success &&
                       (EVP_PKEY_encrypt(_ctx, _ciphertext.data(), &_outLen,
                                         plaintext.data(), plaintext.size()) > 0);

            EVP_PKEY_CTX_free(_ctx);
            EVP_PKEY_free(_pkey);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kEncryptionFailure));
            }

            _ciphertext.resize(_outLen);
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_ciphertext));
        }

        core::Result<std::vector<std::uint8_t>> RsaDecrypt(
            const std::vector<std::uint8_t> &ciphertext,
            const std::vector<std::uint8_t> &privateKeyDer)
        {
            const unsigned char *_derPtr = privateKeyDer.data();
            EVP_PKEY *_pkey = d2i_AutoPrivateKey(
                nullptr, &_derPtr, static_cast<long>(privateKeyDer.size()));
            if (_pkey == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));
            }

            EVP_PKEY_CTX *_ctx = EVP_PKEY_CTX_new(_pkey, nullptr);
            if (_ctx == nullptr)
            {
                EVP_PKEY_free(_pkey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool _success = true;
            _success = _success && (EVP_PKEY_decrypt_init(_ctx) > 0);
            _success = _success && (EVP_PKEY_CTX_set_rsa_padding(
                                        _ctx, RSA_PKCS1_OAEP_PADDING) > 0);

            std::size_t _outLen{0U};
            _success = _success &&
                       (EVP_PKEY_decrypt(_ctx, nullptr, &_outLen,
                                         ciphertext.data(), ciphertext.size()) > 0);

            std::vector<std::uint8_t> _plaintext(_outLen);
            _success = _success &&
                       (EVP_PKEY_decrypt(_ctx, _plaintext.data(), &_outLen,
                                         ciphertext.data(), ciphertext.size()) > 0);

            EVP_PKEY_CTX_free(_ctx);
            EVP_PKEY_free(_pkey);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kDecryptionFailure));
            }

            _plaintext.resize(_outLen);
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_plaintext));
        }
    }
}
