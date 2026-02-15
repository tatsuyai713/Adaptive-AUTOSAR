#include <gtest/gtest.h>
#include "../../../src/ara/crypto/crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        TEST(CryptoErrorDomainTest, DomainName)
        {
            CryptoErrorDomain _domain;
            EXPECT_STREQ(_domain.Name(), "Crypto");
        }

        TEST(CryptoErrorDomainTest, UnsupportedAlgorithmMessage)
        {
            CryptoErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    CryptoErrc::kUnsupportedAlgorithm));
            EXPECT_STREQ(_message, "Unsupported cryptographic algorithm.");
        }

        TEST(CryptoErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            const auto _errorCode = MakeErrorCode(CryptoErrc::kEntropySourceFailure);
            EXPECT_EQ(
                _errorCode.Value(),
                static_cast<core::ErrorDomain::CodeType>(
                    CryptoErrc::kEntropySourceFailure));
            EXPECT_STREQ(_errorCode.Domain().Name(), "Crypto");
        }

        TEST(CryptoErrorDomainTest, InvalidKeySizeMessage)
        {
            CryptoErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    CryptoErrc::kInvalidKeySize));
            EXPECT_STREQ(_message, "Invalid symmetric key length.");
        }

        TEST(CryptoErrorDomainTest, EncryptionFailureMessage)
        {
            CryptoErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    CryptoErrc::kEncryptionFailure));
            EXPECT_STREQ(_message, "Encryption operation failed.");
        }

        TEST(CryptoErrorDomainTest, DecryptionFailureMessage)
        {
            CryptoErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    CryptoErrc::kDecryptionFailure));
            EXPECT_STREQ(_message, "Decryption operation or padding error.");
        }
    }
}
