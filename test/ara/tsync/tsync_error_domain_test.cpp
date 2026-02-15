#include <gtest/gtest.h>
#include "../../../src/ara/tsync/tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        TEST(TsyncErrorDomainTest, DomainName)
        {
            TsyncErrorDomain _domain;
            EXPECT_STREQ(_domain.Name(), "Tsync");
        }

        TEST(TsyncErrorDomainTest, NotSynchronizedMessage)
        {
            TsyncErrorDomain _domain;
            const auto _message =
                _domain.Message(static_cast<core::ErrorDomain::CodeType>(
                    TsyncErrc::kNotSynchronized));
            EXPECT_STREQ(_message, "Time base is not synchronized.");
        }

        TEST(TsyncErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            const auto _errorCode = MakeErrorCode(TsyncErrc::kInvalidArgument);
            EXPECT_EQ(
                _errorCode.Value(),
                static_cast<core::ErrorDomain::CodeType>(
                    TsyncErrc::kInvalidArgument));
            EXPECT_STREQ(_errorCode.Domain().Name(), "Tsync");
        }
    }
}
