#include <gtest/gtest.h>
#include "../../../src/ara/com/e2e_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            TEST(E2EErrcTest, EnumValues)
            {
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kRepeated), 1);
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kWrongSequence), 2);
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kError), 3);
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kNotAvailable), 4);
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kNoNewData), 5);
                EXPECT_EQ(
                    static_cast<core::ErrorDomain::CodeType>(E2EErrc::kStateMachineError), 6);
            }

            TEST(E2EErrorDomainTest, Name)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(domain.Name(), "E2E");
            }

            TEST(E2EErrorDomainTest, MessageRepeated)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kRepeated)),
                    "E2E: counter repeated");
            }

            TEST(E2EErrorDomainTest, MessageWrongSequence)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kWrongSequence)),
                    "E2E: wrong sequence");
            }

            TEST(E2EErrorDomainTest, MessageError)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kError)),
                    "E2E: CRC mismatch");
            }

            TEST(E2EErrorDomainTest, MessageNotAvailable)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kNotAvailable)),
                    "E2E: data not available");
            }

            TEST(E2EErrorDomainTest, MessageNoNewData)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kNoNewData)),
                    "E2E: no new data");
            }

            TEST(E2EErrorDomainTest, MessageStateMachineError)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(
                    domain.Message(static_cast<core::ErrorDomain::CodeType>(
                        E2EErrc::kStateMachineError)),
                    "E2E: state machine error");
            }

            TEST(E2EErrorDomainTest, MessageUnknown)
            {
                E2EErrorDomain domain;
                EXPECT_STREQ(domain.Message(999), "E2E: unknown error");
            }

            TEST(MakeErrorCodeTest, CreatesValidErrorCode)
            {
                auto ec = MakeErrorCode(E2EErrc::kError);
                EXPECT_EQ(ec.Value(),
                           static_cast<core::ErrorDomain::CodeType>(E2EErrc::kError));
                EXPECT_STREQ(ec.Domain().Name(), "E2E");
            }

            TEST(MakeErrorCodeTest, AllErrcValues)
            {
                auto r = MakeErrorCode(E2EErrc::kRepeated);
                auto w = MakeErrorCode(E2EErrc::kWrongSequence);
                auto e = MakeErrorCode(E2EErrc::kError);
                auto na = MakeErrorCode(E2EErrc::kNotAvailable);
                auto nn = MakeErrorCode(E2EErrc::kNoNewData);
                auto sm = MakeErrorCode(E2EErrc::kStateMachineError);

                EXPECT_STREQ(r.Domain().Name(), "E2E");
                EXPECT_STREQ(w.Domain().Name(), "E2E");
                EXPECT_STREQ(e.Domain().Name(), "E2E");
                EXPECT_STREQ(na.Domain().Name(), "E2E");
                EXPECT_STREQ(nn.Domain().Name(), "E2E");
                EXPECT_STREQ(sm.Domain().Name(), "E2E");
            }
        } // namespace e2e
    }
}
