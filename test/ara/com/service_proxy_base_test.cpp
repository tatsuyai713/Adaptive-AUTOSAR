#include <gtest/gtest.h>
#include "../../../src/ara/com/service_proxy_base.h"

namespace ara
{
    namespace com
    {
        TEST(ServiceProxyBaseTest, StopFindServiceWithoutActiveSearchReturnsError)
        {
            auto _result = ServiceProxyBase::StopFindService(
                FindServiceHandle{1U});

            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Com");
        }

        TEST(ServiceProxyBaseTest, LegacyStopFindServiceNoopWithoutActiveSearch)
        {
            ServiceProxyBase::StopFindService();
            SUCCEED();
        }
    }
}
