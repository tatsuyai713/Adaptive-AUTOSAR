#include <gtest/gtest.h>
#include "../../../src/ara/com/service_proxy_base.h"
#include "../../../src/ara/com/instance_identifier.h"

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

        TEST(ServiceProxyBaseTest, StartFindServiceWithSpecifierAndNullHandlerReturnsError)
        {
            auto specifierResult = ara::core::InstanceSpecifier::Create(
                "/ara/com/test/service");
            ASSERT_TRUE(specifierResult.HasValue());

            std::function<void(ServiceHandleContainer<ServiceHandleType>)> handler;
            auto result = ServiceProxyBase::StartFindService(
                handler,
                specifierResult.Value());

            ASSERT_FALSE(result.HasValue());
            EXPECT_STREQ(result.Error().Domain().Name(), "Com");
        }

        TEST(ServiceProxyBaseTest, StartFindServiceWithSpecifierReturnsHandle)
        {
            auto specifierResult = ara::core::InstanceSpecifier::Create(
                "/ara/com/test/service");
            ASSERT_TRUE(specifierResult.HasValue());

            auto startResult = ServiceProxyBase::StartFindService(
                [](ServiceHandleContainer<ServiceHandleType>) {},
                specifierResult.Value());
            ASSERT_TRUE(startResult.HasValue());

            auto stopResult = ServiceProxyBase::StopFindService(startResult.Value());
            EXPECT_TRUE(stopResult.HasValue());
        }

        TEST(ServiceProxyBaseTest, CheckServiceVersionExact)
        {
            ServiceVersion proxy{1, 5};
            ServiceVersion skel{1, 5};
            EXPECT_TRUE(ServiceProxyBase::CheckServiceVersion(
                proxy, skel, VersionCheckPolicy::kExact));
        }

        TEST(ServiceProxyBaseTest, CheckServiceVersionMinorBackward)
        {
            ServiceVersion proxy{1, 5};
            ServiceVersion skel{1, 8};
            EXPECT_TRUE(ServiceProxyBase::CheckServiceVersion(
                proxy, skel, VersionCheckPolicy::kMinorBackward));
        }
    }
}
