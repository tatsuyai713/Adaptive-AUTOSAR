/// @file src/ara/crypto/crypto_context.h
/// @brief CryptoContext base class (SWS_CRYPT_20000).
/// @details Abstract base for all crypto processing contexts.

#ifndef ARA_CRYPTO_CRYPTO_CONTEXT_H
#define ARA_CRYPTO_CRYPTO_CONTEXT_H

#include <cstdint>
#include <string>

namespace ara
{
    namespace crypto
    {
        /// @brief Unique identifier type for cryptographic algorithms.
        using AlgId = std::uint64_t;

        /// @brief Abstract base for all crypto processing contexts (SWS_CRYPT_20000).
        class CryptoContext
        {
        public:
            virtual ~CryptoContext() noexcept = default;

            /// @brief Get the algorithm identifier for this context.
            virtual AlgId GetAlgorithmId() const noexcept = 0;

            /// @brief Get the human-readable algorithm name.
            virtual const std::string &GetAlgorithmName() const noexcept = 0;

        protected:
            CryptoContext() = default;
            CryptoContext(const CryptoContext &) = default;
            CryptoContext &operator=(const CryptoContext &) = default;
        };
    }
}

#endif
