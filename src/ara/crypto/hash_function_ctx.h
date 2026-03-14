/// @file src/ara/crypto/hash_function_ctx.h
/// @brief Incremental (streaming) hash context per AUTOSAR AP SWS_Crypto.
/// @details Provides a stateful Start/Update/Finish interface that wraps
///          the one-shot ComputeDigest for incremental hashing.

#ifndef ARA_CRYPTO_HASH_FUNCTION_CTX_H
#define ARA_CRYPTO_HASH_FUNCTION_CTX_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Streaming hash computation context (SWS_CRYPT_21100).
        /// @details Allows data to be fed incrementally via Update() calls
        ///          before producing the final digest with Finish().
        class HashFunctionCtx
        {
        public:
            /// @brief Create a hash context for a given algorithm.
            explicit HashFunctionCtx(DigestAlgorithm algorithm = DigestAlgorithm::kSha256);

            ~HashFunctionCtx() noexcept;
            HashFunctionCtx(const HashFunctionCtx &) = delete;
            HashFunctionCtx &operator=(const HashFunctionCtx &) = delete;
            HashFunctionCtx(HashFunctionCtx &&other) noexcept;
            HashFunctionCtx &operator=(HashFunctionCtx &&other) noexcept;

            /// @brief (Re-)initialize the context for a fresh hash computation.
            core::Result<void> Start();

            /// @brief Feed data into the running hash.
            core::Result<void> Update(const std::vector<std::uint8_t> &data);

            /// @brief Feed C-style data into the running hash.
            core::Result<void> Update(const void *data, std::size_t size);

            /// @brief Finalize computation and retrieve the digest.
            core::Result<std::vector<std::uint8_t>> Finish();

            /// @brief Get the configured algorithm.
            DigestAlgorithm GetAlgorithm() const noexcept { return mAlgorithm; }

        private:
            DigestAlgorithm mAlgorithm;
            struct Impl;
            Impl *mImpl; ///< Opaque pointer to OpenSSL EVP_MD_CTX
        };
    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_HASH_FUNCTION_CTX_H
