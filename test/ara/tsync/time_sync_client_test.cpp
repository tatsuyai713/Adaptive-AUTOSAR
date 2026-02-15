#include <gtest/gtest.h>
#include "../../../src/ara/tsync/time_sync_client.h"

namespace ara
{
    namespace tsync
    {
        TEST(TimeSyncClientTest, InitialStateIsUnsynchronized)
        {
            TimeSyncClient _client;
            EXPECT_EQ(_client.GetState(), SynchronizationState::kUnsynchronized);
        }

        TEST(TimeSyncClientTest, GetCurrentTimeFailsBeforeSynchronization)
        {
            TimeSyncClient _client;
            const auto _result = _client.GetCurrentTime();

            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Tsync");
        }

        TEST(TimeSyncClientTest, UpdateReferenceAndResolveCurrentTime)
        {
            TimeSyncClient _client;

            const auto _steadyReference = std::chrono::steady_clock::now();
            const auto _globalReference = std::chrono::system_clock::now();
            ASSERT_TRUE(
                _client.UpdateReferenceTime(_globalReference, _steadyReference)
                    .HasValue());

            const auto _resolvedReference = _client.GetCurrentTime(_steadyReference);
            ASSERT_TRUE(_resolvedReference.HasValue());

            const auto _resolvedFuture = _client.GetCurrentTime(
                _steadyReference + std::chrono::milliseconds(100U));
            ASSERT_TRUE(_resolvedFuture.HasValue());

            const auto _delta = _resolvedFuture.Value() - _resolvedReference.Value();
            const auto _deltaMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(_delta).count();
            EXPECT_EQ(_deltaMs, 100);
        }

        TEST(TimeSyncClientTest, ResetReturnsToUnsynchronizedState)
        {
            TimeSyncClient _client;
            ASSERT_TRUE(_client.UpdateReferenceTime(std::chrono::system_clock::now())
                            .HasValue());

            _client.Reset();
            EXPECT_EQ(_client.GetState(), SynchronizationState::kUnsynchronized);

            const auto _result = _client.GetCurrentOffset();
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
