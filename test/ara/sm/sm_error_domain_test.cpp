#include <gtest/gtest.h>
#include "../../../src/ara/sm/sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        TEST(SmErrorDomainTest, DomainName)
        {
            SmErrorDomain _domain;
            EXPECT_STREQ(_domain.Name(), "SM");
        }

        TEST(SmErrorDomainTest, ErrorMessages)
        {
            SmErrorDomain _domain;
            EXPECT_STREQ(
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(SmErrc::kInvalidState)),
                "Operation not permitted in current state.");
            EXPECT_STREQ(
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(SmErrc::kTransitionFailed)),
                "State transition failed.");
            EXPECT_STREQ(
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(SmErrc::kAlreadyInState)),
                "Already in the requested state.");
            EXPECT_STREQ(
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(SmErrc::kNetworkUnavailable)),
                "Network resource is unavailable.");
            EXPECT_STREQ(
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(SmErrc::kInvalidArgument)),
                "Invalid argument supplied.");
            EXPECT_STREQ(_domain.Message(999), "Unknown SM error.");
        }

        TEST(SmErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            auto _code = MakeErrorCode(SmErrc::kInvalidState);
            EXPECT_STREQ(_code.Domain().Name(), "SM");
            EXPECT_EQ(
                _code.Value(),
                static_cast<core::ErrorDomain::CodeType>(SmErrc::kInvalidState));
        }
    }
}
