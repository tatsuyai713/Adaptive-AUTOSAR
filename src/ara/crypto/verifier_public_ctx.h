/// @file src/ara/crypto/verifier_public_ctx.h
/// @brief Streaming public-key signature verifier context (SWS_CRYPT_23300).

#ifndef ARA_CRYPTO_VERIFIER_PUBLIC_CTX_H
#define ARA_CRYPTO_VERIFIER_PUBLIC_CTX_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"
#include "./signer_private_ctx.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Streaming public-key signature verifier context (SWS_CRYPT_23300).
        class VerifierPublicCtx
        {
        public:
            VerifierPublicCtx() noexcept;
            ~VerifierPublicCtx() noexcept;

            VerifierPublicCtx(const VerifierPublicCtx &) = delete;
            VerifierPublicCtx &operator=(const VerifierPublicCtx &) = delete;
            VerifierPublicCtx(VerifierPublicCtx &&other) noexcept;
            VerifierPublicCtx &operator=(VerifierPublicCtx &&other) noexcept;

            /// @brief Start a verification operation.
            /// @param publicKeyPem PEM-encoded public key.
            /// @param hashAlgo Digest algorithm.
            core::Result<void> Start(
                const std::vector<std::uint8_t> &publicKeyPem,
                SignHashAlgorithm hashAlgo = SignHashAlgorithm::kSha256);

            /// @brief Feed data incrementally.
            core::Result<void> Update(
                const std::vector<std::uint8_t> &data);

            /// @brief Finalize and verify the given signature.
            /// @param signature The signature to verify against accumulated data.
            /// @returns True if valid, false if invalid, error on fault.
            core::Result<bool> Finish(
                const std::vector<std::uint8_t> &signature);

            bool IsStarted() const noexcept;

        private:
            struct Impl;
            Impl *mImpl;
        };

    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_VERIFIER_PUBLIC_CTX_H
