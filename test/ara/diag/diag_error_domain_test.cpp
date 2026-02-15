#include <gtest/gtest.h>
#include "../../../src/ara/diag/diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        TEST(DiagErrorDomainTest, NameProperty)
        {
            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};
            EXPECT_STRNE(_errorDomain->Name(), "");
        }

        TEST(DiagErrorDomainTest, MakeErrorCodeMethod)
        {
            const DiagErrc cExpectedResult{DiagErrc::kGenericError};

            ara::core::ErrorDomain *_errorDomain{DiagErrorDomain::GetDiagDomain()};
            auto _diagErrorDomain{static_cast<DiagErrorDomain *>(_errorDomain)};
            ara::core::ErrorCode _errorCode{_diagErrorDomain->MakeErrorCode(cExpectedResult)};
            auto _actualResult{static_cast<DiagErrc>(_errorCode.Value())};

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(DiagErrorDomainTest, FreeFunctionErrorDomainAndMakeErrorCode)
        {
            const auto &_domain{GetDiagErrorDomain()};
            EXPECT_STRNE(_domain.Name(), "");

            const auto _errorCode{MakeErrorCode(DiagErrc::kRejected)};
            EXPECT_EQ(
                DiagErrc::kRejected,
                static_cast<DiagErrc>(_errorCode.Value()));
        }
    }
}
