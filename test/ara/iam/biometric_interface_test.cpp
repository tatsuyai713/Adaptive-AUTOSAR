#include <gtest/gtest.h>
#include "../../../src/ara/iam/biometric_interface.h"

namespace ara
{
    namespace iam
    {
        TEST(BiometricInterfaceTest, EnrolAndHasTemplate)
        {
            BiometricInterface _bio;
            auto _r = _bio.Enrol("user1", BiometricType::kFingerprint, {0x01, 0x02, 0x03});
            EXPECT_TRUE(_r.HasValue());
            EXPECT_TRUE(_bio.HasTemplate("user1"));
        }

        TEST(BiometricInterfaceTest, EnrolDuplicateFails)
        {
            BiometricInterface _bio;
            _bio.Enrol("user1", BiometricType::kFingerprint, {0x01});
            auto _r = _bio.Enrol("user1", BiometricType::kFace, {0x02});
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(BiometricInterfaceTest, RevokeSucceeds)
        {
            BiometricInterface _bio;
            _bio.Enrol("user1", BiometricType::kFingerprint, {0x01});
            auto _r = _bio.Revoke("user1");
            EXPECT_TRUE(_r.HasValue());
            EXPECT_FALSE(_bio.HasTemplate("user1"));
        }

        TEST(BiometricInterfaceTest, RevokeNonexistentFails)
        {
            BiometricInterface _bio;
            auto _r = _bio.Revoke("ghost");
            EXPECT_FALSE(_r.HasValue());
        }

        TEST(BiometricInterfaceTest, VerifyMatchingTemplate)
        {
            BiometricInterface _bio(0.5);
            std::vector<uint8_t> _tmpl = {0xAA, 0xBB, 0xCC};
            _bio.Enrol("user1", BiometricType::kFingerprint, _tmpl);
            auto _result = _bio.Verify(BiometricType::kFingerprint, _tmpl);
            EXPECT_EQ(_result.Result, BiometricMatchResult::kMatch);
            EXPECT_EQ(_result.MatchedIdentityId, "user1");
        }

        TEST(BiometricInterfaceTest, VerifyNoMatch)
        {
            BiometricInterface _bio(0.99);
            _bio.Enrol("user1", BiometricType::kFingerprint, {0x01, 0x02});
            auto _result = _bio.Verify(BiometricType::kFingerprint, {0xFF, 0xFE});
            EXPECT_EQ(_result.Result, BiometricMatchResult::kNoMatch);
        }

        TEST(BiometricInterfaceTest, VerifyEmptyReturnsTemplateNotFound)
        {
            BiometricInterface _bio;
            auto _result = _bio.Verify(BiometricType::kFingerprint, {0x01});
            EXPECT_EQ(_result.Result, BiometricMatchResult::kTemplateNotFound);
        }

        TEST(BiometricInterfaceTest, GetEnrolledIdentities)
        {
            BiometricInterface _bio;
            _bio.Enrol("user1", BiometricType::kFingerprint, {0x01});
            _bio.Enrol("user2", BiometricType::kFace, {0x02});
            auto _ids = _bio.GetEnrolledIdentities();
            EXPECT_EQ(_ids.size(), 2U);
        }

        TEST(BiometricInterfaceTest, GetTemplateCount)
        {
            BiometricInterface _bio;
            EXPECT_EQ(_bio.GetTemplateCount(), 0U);
            _bio.Enrol("user1", BiometricType::kFingerprint, {0x01});
            EXPECT_EQ(_bio.GetTemplateCount(), 1U);
        }
    }
}
