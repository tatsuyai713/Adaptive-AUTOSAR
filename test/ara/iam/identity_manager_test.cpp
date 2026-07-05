#include <gtest/gtest.h>
#include "../../../src/ara/crypto/crypto_provider.h"
#include "../../../src/ara/iam/identity_manager.h"

namespace ara
{
    namespace iam
    {
        TEST(IdentityManagerTest, AuthenticateRequiresDerivedIdentityToken)
        {
            IdentityManager _manager;
            _manager.RegisterIdentity(42U, "AdaptiveApp");

            std::vector<std::uint8_t> _identity{
                'A', 'd', 'a', 'p', 't', 'i', 'v', 'e', 'A', 'p', 'p'};
            auto _credential = ara::crypto::ComputeDigest(
                _identity, ara::crypto::DigestAlgorithm::kSha256);
            ASSERT_TRUE(_credential.HasValue());

            auto _ok = _manager.AuthenticateProcess(
                42U, _credential.Value());
            ASSERT_TRUE(_ok.HasValue());
            EXPECT_TRUE(_ok.Value());

            auto _bad = _manager.AuthenticateProcess(
                42U, {0x01, 0x02, 0x03});
            ASSERT_TRUE(_bad.HasValue());
            EXPECT_FALSE(_bad.Value());
        }

        TEST(IdentityManagerTest, AuthenticateUnknownProcessFails)
        {
            IdentityManager _manager;
            auto _result = _manager.AuthenticateProcess(999U, {0x01});
            ASSERT_TRUE(_result.HasValue());
            EXPECT_FALSE(_result.Value());
        }
    }
}
