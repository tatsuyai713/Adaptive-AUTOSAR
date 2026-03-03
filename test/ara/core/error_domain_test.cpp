#include <gtest/gtest.h>
#include "../../../src/ara/core/error_domain.h"
#include "../../../src/ara/core/core_error_domain.h"

namespace ara
{
    namespace core
    {
        class ErrorDomainTest : public testing::Test, public ErrorDomain
        {
        protected:
            static const IdType cTestDomainId = 1;

        public:
            ErrorDomainTest() : ErrorDomain{cTestDomainId}
            {
            }

            virtual const char *Name() const noexcept override
            {
                return "";
            }

            virtual const char *Message(CodeType errorCode) const noexcept override
            {
                return "";
            }

            void ThrowAsException(const ErrorCode &errorCode) const override
            {
                throw CoreException{errorCode};
            }
        };

        const ErrorDomain::IdType ErrorDomainTest::cTestDomainId;

        TEST_F(ErrorDomainTest, Constructor)
        {
            EXPECT_EQ(cTestDomainId, Id());
        }
    }
}