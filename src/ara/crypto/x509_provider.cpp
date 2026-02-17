/// @file src/ara/crypto/x509_provider.cpp
/// @brief Implementation for X.509 certificate parsing and verification.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./x509_provider.h"

#include <cstring>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            std::string X509NameToString(X509_NAME *name)
            {
                if (name == nullptr)
                {
                    return "";
                }
                char *_buf = X509_NAME_oneline(name, nullptr, 0);
                if (_buf == nullptr)
                {
                    return "";
                }
                std::string _result{_buf};
                OPENSSL_free(_buf);
                return _result;
            }

            std::string AsnTimeToString(const ASN1_TIME *t)
            {
                if (t == nullptr)
                {
                    return "";
                }
                BIO *_bio = BIO_new(BIO_s_mem());
                if (_bio == nullptr)
                {
                    return "";
                }
                ASN1_TIME_print(_bio, t);
                char *_buf = nullptr;
                long _len = BIO_get_mem_data(_bio, &_buf);
                std::string _result{_buf, static_cast<std::size_t>(_len)};
                BIO_free(_bio);
                return _result;
            }

            std::uint64_t AsnTimeToEpoch(const ASN1_TIME *t)
            {
                if (t == nullptr)
                {
                    return 0U;
                }
                struct tm _tm;
                std::memset(&_tm, 0, sizeof(_tm));
                // ASN1_TIME_to_tm returns 1 on success
                if (ASN1_TIME_to_tm(t, &_tm) != 1)
                {
                    return 0U;
                }
                time_t _epoch = mktime(&_tm);
                return (_epoch < 0) ? 0U : static_cast<std::uint64_t>(_epoch);
            }

            std::string SerialToHexString(const ASN1_INTEGER *serial)
            {
                if (serial == nullptr)
                {
                    return "";
                }
                BIGNUM *_bn = ASN1_INTEGER_to_BN(serial, nullptr);
                if (_bn == nullptr)
                {
                    return "";
                }
                char *_hex = BN_bn2hex(_bn);
                BN_free(_bn);
                if (_hex == nullptr)
                {
                    return "";
                }
                std::string _result{_hex};
                OPENSSL_free(_hex);
                return _result;
            }

            X509 *PemToX509(const std::string &pemData)
            {
                BIO *_bio = BIO_new_mem_buf(pemData.data(),
                                             static_cast<int>(pemData.size()));
                if (_bio == nullptr)
                {
                    return nullptr;
                }
                X509 *_cert = PEM_read_bio_X509(_bio, nullptr, nullptr, nullptr);
                BIO_free(_bio);
                return _cert;
            }

            core::Result<X509CertificateInfo> ExtractInfo(X509 *cert)
            {
                if (cert == nullptr)
                {
                    return core::Result<X509CertificateInfo>::FromError(
                        MakeErrorCode(CryptoErrc::kCertificateParseError));
                }

                X509CertificateInfo _info;
                _info.SubjectDn = X509NameToString(X509_get_subject_name(cert));
                _info.IssuerDn = X509NameToString(X509_get_issuer_name(cert));
                _info.SerialNumber = SerialToHexString(
                    X509_get_serialNumber(cert));
                _info.NotBeforeEpochSec = AsnTimeToEpoch(
                    X509_get0_notBefore(cert));
                _info.NotAfterEpochSec = AsnTimeToEpoch(
                    X509_get0_notAfter(cert));

                // Extract public key DER
                EVP_PKEY *_pkey = X509_get_pubkey(cert);
                if (_pkey != nullptr)
                {
                    unsigned char *_buf = nullptr;
                    int _len = i2d_PUBKEY(_pkey, &_buf);
                    if (_len > 0 && _buf != nullptr)
                    {
                        _info.PublicKeyDer.assign(_buf, _buf + _len);
                        OPENSSL_free(_buf);
                    }
                    EVP_PKEY_free(_pkey);
                }

                // Check self-signed
                _info.IsSelfSigned =
                    (X509_check_issued(cert, cert) == X509_V_OK);

                return core::Result<X509CertificateInfo>::FromValue(
                    std::move(_info));
            }
        }

        core::Result<X509CertificateInfo> ParseX509Pem(
            const std::string &pemData)
        {
            X509 *_cert = PemToX509(pemData);
            if (_cert == nullptr)
            {
                return core::Result<X509CertificateInfo>::FromError(
                    MakeErrorCode(CryptoErrc::kCertificateParseError));
            }

            auto _result = ExtractInfo(_cert);
            X509_free(_cert);
            return _result;
        }

        core::Result<X509CertificateInfo> ParseX509Der(
            const std::vector<std::uint8_t> &derData)
        {
            const unsigned char *_ptr = derData.data();
            X509 *_cert = d2i_X509(
                nullptr, &_ptr, static_cast<long>(derData.size()));
            if (_cert == nullptr)
            {
                return core::Result<X509CertificateInfo>::FromError(
                    MakeErrorCode(CryptoErrc::kCertificateParseError));
            }

            auto _result = ExtractInfo(_cert);
            X509_free(_cert);
            return _result;
        }

        core::Result<bool> VerifyX509Chain(
            const std::string &leafPem,
            const std::vector<std::string> &caCertsPem)
        {
            X509 *_leaf = PemToX509(leafPem);
            if (_leaf == nullptr)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kCertificateParseError));
            }

            X509_STORE *_store = X509_STORE_new();
            if (_store == nullptr)
            {
                X509_free(_leaf);
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            for (const auto &_caPem : caCertsPem)
            {
                X509 *_ca = PemToX509(_caPem);
                if (_ca != nullptr)
                {
                    X509_STORE_add_cert(_store, _ca);
                    X509_free(_ca);
                }
            }

            X509_STORE_CTX *_ctx = X509_STORE_CTX_new();
            if (_ctx == nullptr)
            {
                X509_STORE_free(_store);
                X509_free(_leaf);
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            X509_STORE_CTX_init(_ctx, _store, _leaf, nullptr);
            int _verifyResult = X509_verify_cert(_ctx);

            X509_STORE_CTX_free(_ctx);
            X509_STORE_free(_store);
            X509_free(_leaf);

            return core::Result<bool>::FromValue(_verifyResult == 1);
        }
    }
}
