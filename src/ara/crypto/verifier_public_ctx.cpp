/// @file src/ara/crypto/verifier_public_ctx.cpp
/// @brief Implementation of VerifierPublicCtx (SWS_CRYPT_23300).

#include "./verifier_public_ctx.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            const EVP_MD *resolveVerifyMd(SignHashAlgorithm algo)
            {
                switch (algo)
                {
                case SignHashAlgorithm::kSha384:
                    return EVP_sha384();
                case SignHashAlgorithm::kSha512:
                    return EVP_sha512();
                case SignHashAlgorithm::kSha256:
                default:
                    return EVP_sha256();
                }
            }
        }

        struct VerifierPublicCtx::Impl
        {
            EVP_MD_CTX *mdCtx{nullptr};
            bool started{false};

            Impl()
            {
                mdCtx = EVP_MD_CTX_new();
            }

            ~Impl()
            {
                if (mdCtx != nullptr)
                {
                    EVP_MD_CTX_free(mdCtx);
                }
            }
        };

        VerifierPublicCtx::VerifierPublicCtx() noexcept : mImpl{nullptr} {}

        VerifierPublicCtx::~VerifierPublicCtx() noexcept
        {
            delete mImpl;
        }

        VerifierPublicCtx::VerifierPublicCtx(VerifierPublicCtx &&other) noexcept
            : mImpl{other.mImpl}
        {
            other.mImpl = nullptr;
        }

        VerifierPublicCtx &VerifierPublicCtx::operator=(VerifierPublicCtx &&other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        core::Result<void> VerifierPublicCtx::Start(
            const std::vector<std::uint8_t> &publicKeyPem,
            SignHashAlgorithm hashAlgo)
        {
            delete mImpl;
            mImpl = new Impl();
            if (mImpl->mdCtx == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            BIO *bio = BIO_new_mem_buf(publicKeyPem.data(),
                                       static_cast<int>(publicKeyPem.size()));
            if (bio == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (pkey == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            const EVP_MD *md = resolveVerifyMd(hashAlgo);
            int rc = EVP_DigestVerifyInit(mImpl->mdCtx, nullptr, md,
                                          nullptr, pkey);
            EVP_PKEY_free(pkey);

            if (rc != 1)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            mImpl->started = true;
            return core::Result<void>::FromValue();
        }

        core::Result<void> VerifierPublicCtx::Update(
            const std::vector<std::uint8_t> &data)
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            if (EVP_DigestVerifyUpdate(mImpl->mdCtx, data.data(), data.size()) != 1)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<bool> VerifierPublicCtx::Finish(
            const std::vector<std::uint8_t> &signature)
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            int rc = EVP_DigestVerifyFinal(
                mImpl->mdCtx, signature.data(), signature.size());

            mImpl->started = false;

            if (rc == 1)
            {
                return core::Result<bool>::FromValue(true);
            }
            else if (rc == 0)
            {
                return core::Result<bool>::FromValue(false);
            }
            else
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }
        }

        bool VerifierPublicCtx::IsStarted() const noexcept
        {
            return (mImpl != nullptr) && mImpl->started;
        }

    } // namespace crypto
} // namespace ara
