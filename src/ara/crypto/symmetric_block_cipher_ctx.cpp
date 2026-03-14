/// @file src/ara/crypto/symmetric_block_cipher_ctx.cpp
/// @brief Implementation of SymmetricBlockCipherCtx (SWS_CRYPT_23100).

#include "./symmetric_block_cipher_ctx.h"
#include <openssl/evp.h>
#include <cstring>

namespace ara
{
    namespace crypto
    {
        struct SymmetricBlockCipherCtx::Impl
        {
            EVP_CIPHER_CTX *ctx{nullptr};
            bool started{false};

            Impl()
            {
                ctx = EVP_CIPHER_CTX_new();
            }

            ~Impl()
            {
                if (ctx != nullptr)
                {
                    EVP_CIPHER_CTX_free(ctx);
                }
            }
        };

        SymmetricBlockCipherCtx::SymmetricBlockCipherCtx() noexcept
            : mImpl{nullptr}
        {
        }

        SymmetricBlockCipherCtx::~SymmetricBlockCipherCtx() noexcept
        {
            delete mImpl;
        }

        SymmetricBlockCipherCtx::SymmetricBlockCipherCtx(
            SymmetricBlockCipherCtx &&other) noexcept
            : mImpl{other.mImpl}
        {
            other.mImpl = nullptr;
        }

        SymmetricBlockCipherCtx &SymmetricBlockCipherCtx::operator=(
            SymmetricBlockCipherCtx &&other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        core::Result<void> SymmetricBlockCipherCtx::Start(
            CipherDirection direction,
            const std::vector<std::uint8_t> &key,
            const std::vector<std::uint8_t> &iv)
        {
            if (key.size() != 16U && key.size() != 32U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }
            if (iv.size() != 16U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            delete mImpl;
            mImpl = new Impl();
            if (mImpl->ctx == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            const EVP_CIPHER *cipher =
                (key.size() == 16U) ? EVP_aes_128_cbc() : EVP_aes_256_cbc();

            int enc = (direction == CipherDirection::kEncrypt) ? 1 : 0;

            if (EVP_CipherInit_ex(mImpl->ctx, cipher, nullptr,
                                  key.data(), iv.data(), enc) != 1)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            mImpl->started = true;
            return core::Result<void>::FromValue();
        }

        core::Result<std::vector<std::uint8_t>> SymmetricBlockCipherCtx::Update(
            const std::vector<std::uint8_t> &input)
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            std::vector<std::uint8_t> output(input.size() + 16U);
            int outLen = 0;

            if (EVP_CipherUpdate(mImpl->ctx, output.data(), &outLen,
                                 input.data(),
                                 static_cast<int>(input.size())) != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            output.resize(static_cast<std::size_t>(outLen));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(output));
        }

        core::Result<std::vector<std::uint8_t>> SymmetricBlockCipherCtx::Finish()
        {
            if (mImpl == nullptr || !mImpl->started)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kProcessingNotStarted));
            }

            std::vector<std::uint8_t> output(32U);
            int outLen = 0;

            if (EVP_CipherFinal_ex(mImpl->ctx, output.data(), &outLen) != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            output.resize(static_cast<std::size_t>(outLen));
            mImpl->started = false;
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(output));
        }

        bool SymmetricBlockCipherCtx::IsStarted() const noexcept
        {
            return (mImpl != nullptr) && mImpl->started;
        }

    } // namespace crypto
} // namespace ara
