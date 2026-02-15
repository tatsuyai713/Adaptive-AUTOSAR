#include <gtest/gtest.h>
#include "../../../src/ara/exec/deterministic_client.h"

namespace ara
{
    namespace exec
    {
        TEST(DeterministicClientTest, GetRandomMethod)
        {
            DeterministicClient _deterministicClient;
            DeterministicClient _otherDeterministicClient;

            uint64_t _random = _deterministicClient.GetRandom();
            uint64_t _otherRandom = _otherDeterministicClient.GetRandom();
            EXPECT_EQ(_random, _otherRandom);

            _deterministicClient.WaitForActivation();

            _random = _deterministicClient.GetRandom();
            EXPECT_NE(_random, _otherRandom);
        }

        TEST(DeterministicClientTest, TimeStamps)
        {
            DeterministicClient _deterministicClient;

            auto _activationTime = _deterministicClient.GetActivationTime();
            auto _nextActivationTime = _deterministicClient.GetNextActivationTime();
            EXPECT_GT(_nextActivationTime.Value(), _activationTime.Value());

            _deterministicClient.WaitForActivation();
            _activationTime = _deterministicClient.GetActivationTime();
            EXPECT_GE(_activationTime.Value(), _nextActivationTime.Value());
        }

        TEST(DeterministicClientTest, LifecycleProgression)
        {
            DeterministicClient _client;

            auto _result1 = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kRegisterService, _result1.Value());

            auto _result2 = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kServiceDiscovery, _result2.Value());

            auto _result3 = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kInit, _result3.Value());

            auto _result4 = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kRun, _result4.Value());

            // kRun should repeat
            auto _result5 = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kRun, _result5.Value());
        }

        TEST(DeterministicClientTest, RequestTerminateMethod)
        {
            DeterministicClient _client;

            DeterministicClient::RequestTerminate();

            auto _result = _client.WaitForActivation();
            EXPECT_EQ(ActivationReturnType::kTerminate, _result.Value());
        }
    }
}