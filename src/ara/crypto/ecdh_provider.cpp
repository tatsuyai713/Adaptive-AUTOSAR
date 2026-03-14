/// @file src/ara/crypto/ecdh_provider.cpp
/// @brief Implementation of ECDH key agreement and key import/export.

#include "./ecdh_provider.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/bio.h>

namespace ara
{
    namespace crypto
    {
        core::Result<std::vector<std::uint8_t>> EcdhDeriveSecret(
            const std::vector<std::uint8_t> &ownPrivateKeyDer,
            const std::vector<std::uint8_t> &peerPublicKeyDer)
        {
            // Load own private key
            const unsigned char *privPtr = ownPrivateKeyDer.data();
            EVP_PKEY *privKey = d2i_AutoPrivateKey(
                nullptr, &privPtr, static_cast<long>(ownPrivateKeyDer.size()));
            if (!privKey)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));

            // Load peer public key
            const unsigned char *pubPtr = peerPublicKeyDer.data();
            EVP_PKEY *pubKey = d2i_PUBKEY(
                nullptr, &pubPtr, static_cast<long>(peerPublicKeyDer.size()));
            if (!pubKey)
            {
                EVP_PKEY_free(privKey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));
            }

            // Create derivation context
            EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(privKey, nullptr);
            if (!ctx)
            {
                EVP_PKEY_free(privKey);
                EVP_PKEY_free(pubKey);
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            bool ok = true;
            ok = ok && (EVP_PKEY_derive_init(ctx) > 0);
            ok = ok && (EVP_PKEY_derive_set_peer(ctx, pubKey) > 0);

            std::size_t secretLen = 0;
            ok = ok && (EVP_PKEY_derive(ctx, nullptr, &secretLen) > 0);

            std::vector<std::uint8_t> secret(secretLen);
            ok = ok && (EVP_PKEY_derive(ctx, secret.data(), &secretLen) > 0);

            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(privKey);
            EVP_PKEY_free(pubKey);

            if (!ok)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));

            secret.resize(secretLen);
            return core::Result<std::vector<std::uint8_t>>::FromValue(std::move(secret));
        }

        core::Result<std::string> ExportPublicKeyPem(
            const std::vector<std::uint8_t> &publicKeyDer)
        {
            const unsigned char *ptr = publicKeyDer.data();
            EVP_PKEY *pkey = d2i_PUBKEY(
                nullptr, &ptr, static_cast<long>(publicKeyDer.size()));
            if (!pkey)
                return core::Result<std::string>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));

            BIO *bio = BIO_new(BIO_s_mem());
            if (!bio || PEM_write_bio_PUBKEY(bio, pkey) != 1)
            {
                EVP_PKEY_free(pkey);
                if (bio) BIO_free(bio);
                return core::Result<std::string>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            char *data = nullptr;
            long len = BIO_get_mem_data(bio, &data);
            std::string pem(data, static_cast<std::size_t>(len));

            BIO_free(bio);
            EVP_PKEY_free(pkey);
            return core::Result<std::string>::FromValue(std::move(pem));
        }

        core::Result<std::vector<std::uint8_t>> ImportPublicKeyPem(
            const std::string &pem)
        {
            BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
            if (!bio)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));

            EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (!pkey)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidKeyFormat));

            unsigned char *buf = nullptr;
            int len = i2d_PUBKEY(pkey, &buf);
            EVP_PKEY_free(pkey);

            if (len <= 0 || !buf)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));

            std::vector<std::uint8_t> der(buf, buf + len);
            OPENSSL_free(buf);
            return core::Result<std::vector<std::uint8_t>>::FromValue(std::move(der));
        }
    }
}
