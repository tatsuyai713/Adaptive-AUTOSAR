#include "./crypto_provider.h"

#include <limits>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace ara
{
    namespace crypto
    {
        namespace
        {
            const EVP_MD *ResolveDigestAlgorithm(DigestAlgorithm algorithm)
            {
                switch (algorithm)
                {
                case DigestAlgorithm::kSha256:
                    return EVP_sha256();
                default:
                    return nullptr;
                }
            }
        }

        core::Result<std::vector<std::uint8_t>> ComputeDigest(
            const std::vector<std::uint8_t> &data,
            DigestAlgorithm algorithm)
        {
            const EVP_MD *_md{ResolveDigestAlgorithm(algorithm)};
            if (_md == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kUnsupportedAlgorithm));
            }

            EVP_MD_CTX *_context{EVP_MD_CTX_new()};
            if (_context == nullptr)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::vector<std::uint8_t> _digest(static_cast<std::size_t>(EVP_MD_size(_md)));
            unsigned int _digestLength{0U};

            bool _success{true};
            _success = _success && (EVP_DigestInit_ex(_context, _md, nullptr) == 1);
            if (!data.empty())
            {
                _success = _success &&
                           (EVP_DigestUpdate(_context, data.data(), data.size()) == 1);
            }
            _success = _success &&
                       (EVP_DigestFinal_ex(_context, _digest.data(), &_digestLength) == 1);

            EVP_MD_CTX_free(_context);

            if (!_success)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            _digest.resize(static_cast<std::size_t>(_digestLength));
            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_digest));
        }

        core::Result<std::vector<std::uint8_t>> GenerateRandomBytes(
            std::size_t byteCount)
        {
            if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            std::vector<std::uint8_t> _bytes(byteCount, 0U);
            if (byteCount == 0U)
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(_bytes));
            }

            const int _result = RAND_bytes(
                _bytes.data(),
                static_cast<int>(byteCount));
            if (_result != 1)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(CryptoErrc::kEntropySourceFailure));
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(_bytes));
        }
    }
}
