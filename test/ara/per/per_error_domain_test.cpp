#include <gtest/gtest.h>
#include "../../../src/ara/per/per_error_domain.h"

namespace ara
{
    namespace per
    {
        TEST(PerErrorDomainTest, DomainName)
        {
            PerErrorDomain domain;
            EXPECT_STREQ(domain.Name(), "Per");
        }

        TEST(PerErrorDomainTest, PhysicalStorageFailureMessage)
        {
            PerErrorDomain domain;
            auto msg = domain.Message(
                static_cast<core::ErrorDomain::CodeType>(
                    PerErrc::kPhysicalStorageFailure));
            EXPECT_STREQ(msg, "Physical storage hardware error.");
        }

        TEST(PerErrorDomainTest, KeyNotFoundMessage)
        {
            PerErrorDomain domain;
            auto msg = domain.Message(
                static_cast<core::ErrorDomain::CodeType>(
                    PerErrc::kKeyNotFound));
            EXPECT_STREQ(msg, "Requested key does not exist.");
        }

        TEST(PerErrorDomainTest, MakeErrorCodeCreatesValidCode)
        {
            auto errorCode = MakeErrorCode(PerErrc::kOutOfStorageSpace);
            EXPECT_EQ(
                errorCode.Value(),
                static_cast<core::ErrorDomain::CodeType>(
                    PerErrc::kOutOfStorageSpace));
            EXPECT_STREQ(errorCode.Domain().Name(), "Per");
        }

        TEST(PerErrorDomainTest, ErrorCodesAreDistinct)
        {
            auto code1 = MakeErrorCode(PerErrc::kKeyNotFound);
            auto code2 = MakeErrorCode(PerErrc::kResourceBusy);
            EXPECT_NE(code1.Value(), code2.Value());
        }

        TEST(PerErrorDomainTest, UnknownErrorCode)
        {
            PerErrorDomain domain;
            auto msg = domain.Message(99999U);
            EXPECT_STREQ(msg, "Unknown persistency error.");
        }
    }
}
