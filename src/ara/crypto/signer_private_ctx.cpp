/// @file src/ara/crypto/signer_private_ctx.cpp
/// @brief Implementation of SignerPrivateCtx (SWS_CRYPT_23200).

#include "./signer_private_ctx.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <cstring>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            const EVP_MD *resolveSignMd(SignHashAlgorithm algo)
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

        struct SignerPrivateCtx::Impl
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

        SignerPrivateCtx::SignerPrivateCtx() noexcept : mImpl{nullptr} {}

        SignerPrivateCtx::~SignerPrivateCtx() noexcept
        {
            delete mImpl;
        }

        SignerPrivateCtx::SignerPrivateCtx(SignerPrivateCtx &&other) noexcept
            : mImpl{other.mImpl}
        {
            other.mImpl = nullptr;
        }

        SignerPrivateCtx &SignerPrivateCtx::operator=(SignerPrivateCtx &&other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        core::Result<void> SignerPrivateCtx::Start(
            const std::vector<std::uint8_t> &privateKeyPem,
            SignHashAlgorithm hashAlgo)
        {
            delete mImpl;
            mImpl = new Impl();
            if (mImpl->mdCtx == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            BIO *bio = BIO_new_mem_buf(privateKeyPem.data(),
                                       static_cast<int>(privateKeyPem.size()));
            if (bio == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (pkey == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            const EVP_MD *md = resolveSignMd(hashAlgo);
            int rc = EVP_DigestSignInit(mImpl->mdCtx, nullptr, md,
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

        core::Result<void> SignerPrivateCtx::Update(
            const std::vector<std::uint8_t> &data)
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            if (EVP_DigestSignUpdate(mImpl->mdCtx, data.data(), data.size()) != 1)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<std::vector<std::uint8_t>> SignerPrivateCtx::Finish()
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            std::size_t sigLen{0U};
            if (EVP_DigestSignFinal(mImpl->mdCtx, nullptr, &sigLen) != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::vector<std::uint8_t> signature(sigLen);
            if (EVP_DigestSignFinal(mImpl->mdCtx, signature.data(), &sigLen) != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            signature.resize(sigLen);
            mImpl->started = false;
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(signature));
        }

        bool SignerPrivateCtx::IsStarted() const noexcept
        {
            return (mImpl != nullptr) && mImpl->started;
        }

    } // namespace crypto
} // namespace ara
