/// @file src/ara/crypto/hash_function_ctx.cpp
/// @brief Implementation of incremental hash context using OpenSSL EVP_MD_CTX.

#include "./hash_function_ctx.h"
#include <openssl/evp.h>
#include <cstring>

namespace ara
{
    namespace crypto
    {
        struct HashFunctionCtx::Impl
        {
            EVP_MD_CTX *ctx{nullptr};
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

        HashFunctionCtx::HashFunctionCtx(DigestAlgorithm algorithm)
            : mAlgorithm{algorithm}, mImpl{new Impl}
        {
            mImpl->md = lookupMd(algorithm);
            mImpl->ctx = EVP_MD_CTX_new();
        }

        HashFunctionCtx::~HashFunctionCtx() noexcept
        {
            if (mImpl)
            {
                if (mImpl->ctx) EVP_MD_CTX_free(mImpl->ctx);
                delete mImpl;
            }
        }

        HashFunctionCtx::HashFunctionCtx(HashFunctionCtx &&other) noexcept
            : mAlgorithm{other.mAlgorithm}, mImpl{other.mImpl}
        {
            other.mImpl = nullptr;
        }

        HashFunctionCtx &HashFunctionCtx::operator=(HashFunctionCtx &&other) noexcept
        {
            if (this != &other)
            {
                if (mImpl)
                {
                    if (mImpl->ctx) EVP_MD_CTX_free(mImpl->ctx);
                    delete mImpl;
                }
                mAlgorithm = other.mAlgorithm;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        core::Result<void> HashFunctionCtx::Start()
        {
            if (!mImpl || !mImpl->ctx)
                return core::Result<void>::FromError(MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            if (EVP_DigestInit_ex(mImpl->ctx, mImpl->md, nullptr) != 1)
                return core::Result<void>::FromError(MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            mImpl->started = true;
            return core::Result<void>{};
        }

        core::Result<void> HashFunctionCtx::Update(const std::vector<std::uint8_t> &data)
        {
            return Update(data.data(), data.size());
        }

        core::Result<void> HashFunctionCtx::Update(const void *data, std::size_t size)
        {
            if (!mImpl || !mImpl->started)
                return core::Result<void>::FromError(MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            if (EVP_DigestUpdate(mImpl->ctx, data, size) != 1)
                return core::Result<void>::FromError(MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            return core::Result<void>{};
        }

        core::Result<std::vector<std::uint8_t>> HashFunctionCtx::Finish()
        {
            if (!mImpl || !mImpl->started)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            unsigned int len = 0;
            std::vector<std::uint8_t> digest(EVP_MAX_MD_SIZE);
            if (EVP_DigestFinal_ex(mImpl->ctx, digest.data(), &len) != 1)
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            digest.resize(len);
            mImpl->started = false;
            return core::Result<std::vector<std::uint8_t>>::FromValue(std::move(digest));
        }
    }
}
