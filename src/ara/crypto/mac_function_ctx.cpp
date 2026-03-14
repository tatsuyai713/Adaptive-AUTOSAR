/// @file src/ara/crypto/mac_function_ctx.cpp
/// @brief Implementation of incremental HMAC context using OpenSSL HMAC API.

#include "./mac_function_ctx.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cstring>

namespace ara
{
    namespace crypto
    {
        struct MessageAuthenticationCodeCtx::Impl
        {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            EVP_MAC *mac{nullptr};
            EVP_MAC_CTX *ctx{nullptr};
#else
            HMAC_CTX *ctx{nullptr};
#endif
            const EVP_MD *md{nullptr};
            bool started{false};
        };

        static const EVP_MD *lookupMd(DigestAlgorithm alg)
        {
            switch (alg)
            {
            case DigestAlgorithm::kSha1:   return EVP_sha1();
            case DigestAlgorithm::kSha256: return EVP_sha256();
            case DigestAlgorithm::kSha384: return EVP_sha384();
            case DigestAlgorithm::kSha512: return EVP_sha512();
            default:                       return EVP_sha256();
            }
        }

        MessageAuthenticationCodeCtx::MessageAuthenticationCodeCtx(DigestAlgorithm algorithm)
            : mAlgorithm{algorithm}, mImpl{new Impl}
        {
            mImpl->md = lookupMd(algorithm);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            mImpl->mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
            if (mImpl->mac)
                mImpl->ctx = EVP_MAC_CTX_new(mImpl->mac);
#else
            mImpl->ctx = HMAC_CTX_new();
#endif
        }

        MessageAuthenticationCodeCtx::~MessageAuthenticationCodeCtx() noexcept
        {
            if (mImpl)
            {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
                if (mImpl->ctx) EVP_MAC_CTX_free(mImpl->ctx);
                if (mImpl->mac) EVP_MAC_free(mImpl->mac);
#else
                if (mImpl->ctx) HMAC_CTX_free(mImpl->ctx);
#endif
                delete mImpl;
            }
        }

        MessageAuthenticationCodeCtx::MessageAuthenticationCodeCtx(
            MessageAuthenticationCodeCtx &&other) noexcept
            : mAlgorithm{other.mAlgorithm}, mImpl{other.mImpl}
        {
            other.mImpl = nullptr;
        }

        MessageAuthenticationCodeCtx &MessageAuthenticationCodeCtx::operator=(
            MessageAuthenticationCodeCtx &&other) noexcept
        {
            if (this != &other)
            {
                if (mImpl)
                {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
                    if (mImpl->ctx) EVP_MAC_CTX_free(mImpl->ctx);
                    if (mImpl->mac) EVP_MAC_free(mImpl->mac);
#else
                    if (mImpl->ctx) HMAC_CTX_free(mImpl->ctx);
#endif
                    delete mImpl;
                }
                mAlgorithm = other.mAlgorithm;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        core::Result<void> MessageAuthenticationCodeCtx::Start(
            const std::vector<std::uint8_t> &key)
        {
            if (!mImpl) return core::Result<void>::FromError(
                MakeErrorCode(CryptoErrc::kProcessingNotStarted));

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            if (!mImpl->ctx) return core::Result<void>::FromError(
                MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            const char *mdName = EVP_MD_get0_name(mImpl->md);
            OSSL_PARAM params[2];
            params[0] = OSSL_PARAM_construct_utf8_string(
                "digest", const_cast<char *>(mdName), 0);
            params[1] = OSSL_PARAM_construct_end();
            if (EVP_MAC_init(mImpl->ctx, key.data(), key.size(), params) != 1)
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
#else
            if (HMAC_Init_ex(mImpl->ctx, key.data(), static_cast<int>(key.size()),
                             mImpl->md, nullptr) != 1)
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
#endif
            mImpl->started = true;
            return core::Result<void>{};
        }

        core::Result<void> MessageAuthenticationCodeCtx::Update(
            const std::vector<std::uint8_t> &data)
        {
            return Update(data.data(), data.size());
        }

        core::Result<void> MessageAuthenticationCodeCtx::Update(
            const void *data, std::size_t size)
        {
            if (!mImpl || !mImpl->started)
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            if (EVP_MAC_update(mImpl->ctx, static_cast<const unsigned char *>(data), size) != 1)
#else
            if (HMAC_Update(mImpl->ctx, static_cast<const unsigned char *>(data), size) != 1)
#endif
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            return core::Result<void>{};
        }

        core::Result<std::vector<std::uint8_t>> MessageAuthenticationCodeCtx::Finish()
        {
            if (!mImpl || !mImpl->started)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            std::size_t len = 0;
            EVP_MAC_final(mImpl->ctx, nullptr, &len, 0);
            std::vector<std::uint8_t> tag(len);
            if (EVP_MAC_final(mImpl->ctx, tag.data(), &len, tag.size()) != 1)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            tag.resize(len);
#else
            unsigned int len = 0;
            std::vector<std::uint8_t> tag(EVP_MAX_MD_SIZE);
            if (HMAC_Final(mImpl->ctx, tag.data(), &len) != 1)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            tag.resize(len);
#endif
            mImpl->started = false;
            return core::Result<std::vector<std::uint8_t>>::FromValue(std::move(tag));
        }

        core::Result<std::vector<std::uint8_t>>
        MessageAuthenticationCodeCtx::FinishTruncated(size_t truncatedLength)
        {
            auto fullResult = Finish();
            if (!fullResult.HasValue())
            {
                return fullResult;
            }

            auto tag = fullResult.Value();
            if (truncatedLength == 0U || truncatedLength > tag.size())
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            tag.resize(truncatedLength);
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(tag));
        }
    }
}
