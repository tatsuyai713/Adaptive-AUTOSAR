/// @file src/ara/crypto/symmetric_block_cipher_ctx.h
/// @brief Streaming symmetric block cipher context (SWS_CRYPT_23100).

#ifndef ARA_CRYPTO_SYMMETRIC_BLOCK_CIPHER_CTX_H
#define ARA_CRYPTO_SYMMETRIC_BLOCK_CIPHER_CTX_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Cipher direction.
        enum class CipherDirection : std::uint8_t
        {
            kEncrypt = 0,
            kDecrypt = 1
        };

        /// @brief Streaming symmetric block cipher context (SWS_CRYPT_23100).
        /// @details Wraps AES-CBC with incremental Update/Finish semantics.
        class SymmetricBlockCipherCtx
        {
        public:
            SymmetricBlockCipherCtx() noexcept;
            ~SymmetricBlockCipherCtx() noexcept;

            SymmetricBlockCipherCtx(const SymmetricBlockCipherCtx &) = delete;
            SymmetricBlockCipherCtx &operator=(const SymmetricBlockCipherCtx &) = delete;
            SymmetricBlockCipherCtx(SymmetricBlockCipherCtx &&other) noexcept;
            SymmetricBlockCipherCtx &operator=(SymmetricBlockCipherCtx &&other) noexcept;

            /// @brief Start a cipher operation.
            /// @param direction Encrypt or decrypt.
            /// @param key AES key (16 or 32 bytes).
            /// @param iv Initialization vector (16 bytes).
            core::Result<void> Start(
                CipherDirection direction,
                const std::vector<std::uint8_t> &key,
                const std::vector<std::uint8_t> &iv);

            /// @brief Feed data incrementally.
            /// @param input Input block.
            /// @returns Output bytes produced so far.
            core::Result<std::vector<std::uint8_t>> Update(
                const std::vector<std::uint8_t> &input);

            /// @brief Finalize the cipher operation (handles padding).
            /// @returns Final output bytes.
            core::Result<std::vector<std::uint8_t>> Finish();

            /// @brief Check if the context has been started.
            bool IsStarted() const noexcept;

        private:
            struct Impl;
            Impl *mImpl;
        };

    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_SYMMETRIC_BLOCK_CIPHER_CTX_H
