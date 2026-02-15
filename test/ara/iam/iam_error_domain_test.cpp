#include <gtest/gtest.h>
#include "../../../src/ara/iam/iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        TEST(IamErrorDomainTest, DomainName)
        {
            IamErrorDomain _domain;
            EXPECT_STREQ(_domain.Name(), "Iam");
        }

        TEST(IamErrorDomainTest, InvalidArgumentMessage)
        {
            IamErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    IamErrc::kInvalidArgument));
            EXPECT_STREQ(_message, "Invalid IAM argument.");
        }

        TEST(IamErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            const auto _errorCode = MakeErrorCode(IamErrc::kPolicyStoreError);
            EXPECT_EQ(
                _errorCode.Value(),
                static_cast<core::ErrorDomain::CodeType>(
                    IamErrc::kPolicyStoreError));
            EXPECT_STREQ(_errorCode.Domain().Name(), "Iam");
        }

        TEST(IamErrorDomainTest, PolicyFileParseErrorMessage)
        {
            IamErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    IamErrc::kPolicyFileParseError));
            EXPECT_STREQ(_message, "Policy file format is invalid.");
        }
    }
}
