/// @file src/ara/crypto/signer_private_ctx.h
/// @brief Streaming digital signature context (SWS_CRYPT_23200).

#ifndef ARA_CRYPTO_SIGNER_PRIVATE_CTX_H
#define ARA_CRYPTO_SIGNER_PRIVATE_CTX_H

#include <cstdint>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Digest algorithm for signing/verification.
        enum class SignHashAlgorithm : std::uint8_t
        {
            kSha256 = 0,
            kSha384 = 1,
            kSha512 = 2
        };

        /// @brief Streaming private-key signer context (SWS_CRYPT_23200).
        class SignerPrivateCtx
        {
        public:
            SignerPrivateCtx() noexcept;
            ~SignerPrivateCtx() noexcept;

            SignerPrivateCtx(const SignerPrivateCtx &) = delete;
            SignerPrivateCtx &operator=(const SignerPrivateCtx &) = delete;
            SignerPrivateCtx(SignerPrivateCtx &&other) noexcept;
            SignerPrivateCtx &operator=(SignerPrivateCtx &&other) noexcept;

            /// @brief Start a signing operation.
            /// @param privateKeyPem PEM-encoded private key.
            /// @param hashAlgo Digest algorithm.
            core::Result<void> Start(
                const std::vector<std::uint8_t> &privateKeyPem,
                SignHashAlgorithm hashAlgo = SignHashAlgorithm::kSha256);

            /// @brief Feed data incrementally.
            core::Result<void> Update(
                const std::vector<std::uint8_t> &data);

            /// @brief Finalize and produce the signature.
            core::Result<std::vector<std::uint8_t>> Finish();

            bool IsStarted() const noexcept;

        private:
            struct Impl;
            Impl *mImpl;
        };

    } // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_SIGNER_PRIVATE_CTX_H
