/// @file src/ara/crypto/mac_function_ctx.h
/// @brief Incremental MAC (HMAC) context per AUTOSAR AP SWS_Crypto.
/// @details Provides a stateful Start/Update/Finish interface for HMAC computation.

#ifndef ARA_CRYPTO_MAC_FUNCTION_CTX_H
#define ARA_CRYPTO_MAC_FUNCTION_CTX_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./crypto_provider.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Streaming HMAC computation context (SWS_CRYPT_22100).
        class MessageAuthenticationCodeCtx
        {
        public:
            explicit MessageAuthenticationCodeCtx(
                DigestAlgorithm algorithm = DigestAlgorithm::kSha256);

            ~MessageAuthenticationCodeCtx() noexcept;
            MessageAuthenticationCodeCtx(const MessageAuthenticationCodeCtx &) = delete;
            MessageAuthenticationCodeCtx &operator=(const MessageAuthenticationCodeCtx &) = delete;
            MessageAuthenticationCodeCtx(MessageAuthenticationCodeCtx &&other) noexcept;
            MessageAuthenticationCodeCtx &operator=(MessageAuthenticationCodeCtx &&other) noexcept;

            /// @brief Initialize HMAC with a key.
            core::Result<void> Start(const std::vector<std::uint8_t> &key);

            /// @brief Feed data into the running HMAC.
            core::Result<void> Update(const std::vector<std::uint8_t> &data);

            /// @brief Feed C-style data.
            core::Result<void> Update(const void *data, std::size_t size);

            /// @brief Finalize and return the MAC tag.
            core::Result<std::vector<std::uint8_t>> Finish();

            DigestAlgorithm GetAlgorithm() const noexcept { return mAlgorithm; }

        private:
            DigestAlgorithm mAlgorithm;
            struct Impl;
            Impl *mImpl;
        };
    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_MAC_FUNCTION_CTX_H
