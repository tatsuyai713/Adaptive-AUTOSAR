#include <gtest/gtest.h>
#include "../../../src/ara/phm/supervised_entity.h"
#include "../../../src/ara/phm/phm_error_domain.h"
#include "./mocked_checkpoint_communicator.h"

namespace ara
{
    namespace phm
    {
        enum class DummyCheckpoint : uint32_t
        {
            None = 0,
            Startup = 1
        };

        TEST(SupervisedEntityTest, ReportCheckpointMehod)
        {
            const core::InstanceSpecifier cInstance("Instance0");
            const DummyCheckpoint cExpectedResult{DummyCheckpoint::Startup};

            MockedCheckpointCommunicator _communicator;
            DummyCheckpoint _actualResult{DummyCheckpoint::None};
            auto _callback =
                [&](uint32_t checkpointId) noexcept
            { _actualResult = static_cast<DummyCheckpoint>(checkpointId); };
            auto _setCallbackResult{
                _communicator.SetCallback(_callback)};
            ASSERT_TRUE(_setCallbackResult.HasValue());

            SupervisedEntity _se(cInstance, &_communicator);
            auto _reportResult{
                _se.ReportCheckpoint(cExpectedResult)};
            ASSERT_TRUE(_reportResult.HasValue());

            EXPECT_EQ(cExpectedResult, _actualResult);
        }

        TEST(SupervisedEntityTest, ConstructorRejectsNullCommunicator)
        {
            const core::InstanceSpecifier cInstance("InstanceNull");
            EXPECT_THROW(
                SupervisedEntity(cInstance, nullptr),
                std::invalid_argument);
        }

        TEST(SupervisedEntityTest, SetCallbackRejectsEmptyHandler)
        {
            MockedCheckpointCommunicator _communicator;
            auto _result{_communicator.SetCallback({})};
            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(
                PhmErrc::kInvalidArgument,
                static_cast<PhmErrc>(_result.Error().Value()));
        }

        TEST(SupervisedEntityTest, ReportCheckpointReturnsErrorOnSendFailure)
        {
            const core::InstanceSpecifier cInstance("InstanceNoCallback");
            const DummyCheckpoint cCheckpoint{DummyCheckpoint::Startup};

            MockedCheckpointCommunicator _communicator;
            SupervisedEntity _se(cInstance, &_communicator);
            auto _result{_se.ReportCheckpoint(cCheckpoint)};
            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(
                PhmErrc::kCheckpointCommunicationError,
                static_cast<PhmErrc>(_result.Error().Value()));
        }
    }
}
