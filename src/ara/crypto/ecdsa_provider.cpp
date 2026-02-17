/// @file src/ara/crypto/ecdsa_provider.cpp
/// @brief Implementation for ECDSA asymmetric cryptography provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./ecdsa_provider.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            int ResolveNid(EllipticCurve curve)
            {
                switch (curve)
                {
                case EllipticCurve::kP256:
                    return NID_X9_62_prime256v1;
                case EllipticCurve::kP384:
                    return NID_secp384r1;
                default:
                    return 0;
                }
            }

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

        core::Result<EcKeyPair> GenerateEcKeyPair(EllipticCurve curve)
        {
            int _nid = ResolveNid(curve);
            if (_nid == 0)
            {
                return core::Result<EcKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_PKEY_CTX *_paramCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
            if (_paramCtx == nullptr)
            {
                return core::Result<EcKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
            }

            EVP_PKEY *_pkey = nullptr;
            bool _success = true;
            _success = _success && (EVP_PKEY_keygen_init(_paramCtx) > 0);
            _success = _success && (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(
                                        _paramCtx, _nid) > 0);
            _success = _success && (EVP_PKEY_keygen(_paramCtx, &_pkey) > 0);
            EVP_PKEY_CTX_free(_paramCtx);

            if (!_success || _pkey == nullptr)
            {
                if (_pkey != nullptr)
                {
                    EVP_PKEY_free(_pkey);
                }
                return core::Result<EcKeyPair>::FromError(
                    MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
            }

            EcKeyPair _result;

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
                    return core::Result<EcKeyPair>::FromError(
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
                    return core::Result<EcKeyPair>::FromError(
                        MakeErrorCode(CryptoErrc::kKeyGenerationFailure));
                }
            }

            EVP_PKEY_free(_pkey);
            return core::Result<EcKeyPair>::FromValue(std::move(_result));
        }

        core::Result<std::vector<std::uint8_t>> EcdsaSign(
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

        core::Result<bool> EcdsaVerify(
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
    }
}
