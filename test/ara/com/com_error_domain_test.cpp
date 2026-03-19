#include <gtest/gtest.h>
#include "../../../src/ara/com/com_error_domain.h"

namespace ara
{
    namespace com
    {
        TEST(ComErrorDomainTest, Name)
        {
            ComErrorDomain _domain;
            EXPECT_STREQ("Com", _domain.Name());
        }

        TEST(ComErrorDomainTest, ServiceNotAvailableMessage)
        {
            ComErrorDomain _domain;
            auto _code = static_cast<core::ErrorDomain::CodeType>(
                ComErrc::kServiceNotAvailable);
            EXPECT_STREQ("Service is not available.", _domain.Message(_code));
        }

        TEST(ComErrorDomainTest, NetworkBindingFailureMessage)
        {
            ComErrorDomain _domain;
            auto _code = static_cast<core::ErrorDomain::CodeType>(
                ComErrc::kNetworkBindingFailure);
            EXPECT_STREQ("Network binding could not be created.", _domain.Message(_code));
        }

        TEST(ComErrorDomainTest, ServiceNotOfferedMessage)
        {
            ComErrorDomain _domain;
            auto _code = static_cast<core::ErrorDomain::CodeType>(
                ComErrc::kServiceNotOffered);
            EXPECT_STREQ("Service is not offered.", _domain.Message(_code));
        }

        TEST(ComErrorDomainTest, UnknownErrorMessage)
        {
            ComErrorDomain _domain;
            EXPECT_STREQ("Unknown communication error.", _domain.Message(99));
        }

        TEST(ComErrorDomainTest, SerializationErrorMessage)
        {
            ComErrorDomain _domain;
            auto _code = static_cast<core::ErrorDomain::CodeType>(
                ComErrc::kSerializationError);
            EXPECT_STREQ("Serialization error.", _domain.Message(_code));
        }

        TEST(ComErrorDomainTest, SerializationErrorEnumValue)
        {
            EXPECT_EQ(
                static_cast<core::ErrorDomain::CodeType>(
                    ComErrc::kSerializationError),
                20);
        }

        TEST(ComErrorDomainTest, ErroneousFileHandleMessage)
        {
            ComErrorDomain _domain;
            auto _code = static_cast<core::ErrorDomain::CodeType>(
                ComErrc::kErroneousFileHandle);
            EXPECT_STREQ("Erroneous file handle.", _domain.Message(_code));
        }

        TEST(ComErrorDomainTest, MakeErrorCodeSerializationError)
        {
            auto ec = MakeErrorCode(ComErrc::kSerializationError);
            EXPECT_EQ(ec.Value(), 20);
            EXPECT_STREQ(ec.Domain().Name(), "Com");
        }
    }
}
