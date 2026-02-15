#include <gtest/gtest.h>

#include "../../../src/ara/ucm/ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        TEST(UcmErrorDomainTest, DomainName)
        {
            UcmErrorDomain _domain;
            EXPECT_STREQ(_domain.Name(), "Ucm");
        }

        TEST(UcmErrorDomainTest, VerificationFailureMessage)
        {
            UcmErrorDomain _domain;
            const auto _message = _domain.Message(
                static_cast<core::ErrorDomain::CodeType>(
                    UcmErrc::kVerificationFailed));
            EXPECT_STREQ(_message, "Software package verification failed.");
        }

        TEST(UcmErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            const auto _errorCode = MakeErrorCode(UcmErrc::kInvalidState);
            EXPECT_STREQ(_errorCode.Domain().Name(), "Ucm");
            EXPECT_EQ(
                _errorCode.Value(),
                static_cast<core::ErrorDomain::CodeType>(UcmErrc::kInvalidState));
        }

        TEST(UcmErrorDomainTest, DowngradeMessage)
        {
            UcmErrorDomain _domain;
            const auto _message = _domain.Message(
                static_cast<core::ErrorDomain::CodeType>(
                    UcmErrc::kDowngradeNotAllowed));
            EXPECT_STREQ(_message, "Software downgrade is not allowed.");
        }
    }
}
