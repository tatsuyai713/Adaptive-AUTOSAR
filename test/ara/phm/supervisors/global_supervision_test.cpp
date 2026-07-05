#include <gtest/gtest.h>
#include "./dummy_supervision.h"
#include "../../../../src/ara/phm/supervisors/global_supervision.h"

namespace ara
{
    namespace phm
    {
        namespace supervisors
        {
            TEST(GlobalSupervisionTest, EmptyConstructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kDeactivated};

                const GlobalSupervision cSupervision({});
                const SupervisionStatus cActualResult{cSupervision.GetStatus()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(GlobalSupervisionTest, Constructor)
            {
                const SupervisionStatus cExpectedResult{SupervisionStatus::kOk};

                DummySupervision _okSupervision;
                _okSupervision.ReportStatus(cExpectedResult);

                const GlobalSupervision cGlobalSupervision({&_okSupervision});
                const SupervisionStatus cActualResult{cGlobalSupervision.GetStatus()};

                EXPECT_EQ(cExpectedResult, cActualResult);
            }

            TEST(GlobalSupervisionTest, MutipleSupervisionsScenario)
            {
                DummySupervision _okSupervision;
                DummySupervision _failedSupervision;
                DummySupervision _expiredSupervision;

                const GlobalSupervision cGlobalSupervision(
                    {&_okSupervision, &_failedSupervision, &_expiredSupervision});

                SupervisionStatus _expectedResult{SupervisionStatus::kOk};
                _okSupervision.ReportStatus(_expectedResult);
                SupervisionStatus _actualResult{cGlobalSupervision.GetStatus()};
                EXPECT_EQ(_expectedResult, _actualResult);

                _expectedResult = SupervisionStatus::kFailed;
                _failedSupervision.ReportStatus(_expectedResult);
                _actualResult = cGlobalSupervision.GetStatus();
                EXPECT_EQ(_expectedResult, _actualResult);

                _expectedResult = SupervisionStatus::kExpired;
                _expiredSupervision.ReportStatus(_expectedResult);
                _actualResult = cGlobalSupervision.GetStatus();
                EXPECT_EQ(_expectedResult, _actualResult);
            }

            TEST(GlobalSupervisionTest, RejectsNullSupervisor)
            {
                EXPECT_THROW(
                    GlobalSupervision({static_cast<ElementarySupervision *>(nullptr)}),
                    std::invalid_argument);
            }

            TEST(GlobalSupervisionTest, DestructorResetsElementaryCallback)
            {
                DummySupervision _supervision;
                bool _callbackInvoked{false};

                {
                    GlobalSupervision _globalSupervision({&_supervision});
                    _globalSupervision.SetCallback(
                        [&](SupervisionUpdate) {
                            _callbackInvoked = true;
                        });

                    _supervision.ReportStatus(SupervisionStatus::kOk);
                    EXPECT_TRUE(_callbackInvoked);
                }

                _callbackInvoked = false;
                _supervision.ReportStatus(SupervisionStatus::kFailed);
                EXPECT_FALSE(_callbackInvoked);
            }
        }
    }
}
