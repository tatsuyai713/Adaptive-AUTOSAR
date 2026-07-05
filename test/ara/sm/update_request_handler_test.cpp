#include <gtest/gtest.h>
#include "../../../src/ara/sm/update_request_handler.h"

namespace ara
{
    namespace sm
    {
        TEST(UpdateRequestHandlerTest, VerifyRequiresPreparedUpdate)
        {
            UpdateRequestHandler _handler;

            auto _result = _handler.VerifyUpdate();

            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(_handler.GetUpdateState(), UpdateState::kIdle);
        }

        TEST(UpdateRequestHandlerTest, PrepareVerifyCompletesUpdate)
        {
            UpdateRequestHandler _handler;

            ASSERT_TRUE(_handler.PrepareUpdate().HasValue());
            EXPECT_EQ(_handler.GetUpdateState(), UpdateState::kPending);

            ASSERT_TRUE(_handler.VerifyUpdate().HasValue());
            EXPECT_EQ(_handler.GetUpdateState(), UpdateState::kCompleted);
        }

        TEST(UpdateRequestHandlerTest, RollbackRequiresActiveUpdate)
        {
            UpdateRequestHandler _handler;

            auto _result = _handler.RollbackUpdate();

            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(_handler.GetUpdateState(), UpdateState::kIdle);
        }

        TEST(UpdateRequestHandlerTest, CallbackCanQueryState)
        {
            UpdateRequestHandler _handler;
            bool _queried{false};

            _handler.SetUpdateCallback(
                [&](UpdateState state) {
                    _queried = true;
                    EXPECT_EQ(_handler.GetUpdateState(), state);
                });

            ASSERT_TRUE(_handler.PrepareUpdate().HasValue());
            EXPECT_TRUE(_queried);
        }
    }
}
