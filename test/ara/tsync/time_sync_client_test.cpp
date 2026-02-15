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

        TEST(TimeSyncClientTest, StateChangeNotifierOnSynchronization)
        {
            TimeSyncClient _client;
            SynchronizationState _capturedState{SynchronizationState::kUnsynchronized};
            int _callCount{0};

            auto _result = _client.SetStateChangeNotifier(
                [&](SynchronizationState state)
                {
                    _capturedState = state;
                    ++_callCount;
                });
            ASSERT_TRUE(_result.HasValue());

            _client.UpdateReferenceTime(std::chrono::system_clock::now());
            EXPECT_EQ(_capturedState, SynchronizationState::kSynchronized);
            EXPECT_EQ(_callCount, 1);

            // Second update should not fire again (already synchronized)
            _client.UpdateReferenceTime(std::chrono::system_clock::now());
            EXPECT_EQ(_callCount, 1);
        }

        TEST(TimeSyncClientTest, StateChangeNotifierOnReset)
        {
            TimeSyncClient _client;
            SynchronizationState _capturedState{SynchronizationState::kSynchronized};
            int _callCount{0};

            _client.UpdateReferenceTime(std::chrono::system_clock::now());

            _client.SetStateChangeNotifier(
                [&](SynchronizationState state)
                {
                    _capturedState = state;
                    ++_callCount;
                });

            _client.Reset();
            EXPECT_EQ(_capturedState, SynchronizationState::kUnsynchronized);
            EXPECT_EQ(_callCount, 1);

            // Second reset should not fire again
            _client.Reset();
            EXPECT_EQ(_callCount, 1);
        }

        TEST(TimeSyncClientTest, ClearStateChangeNotifier)
        {
            TimeSyncClient _client;
            int _callCount{0};

            _client.SetStateChangeNotifier(
                [&](SynchronizationState)
                {
                    ++_callCount;
                });
            _client.ClearStateChangeNotifier();

            _client.UpdateReferenceTime(std::chrono::system_clock::now());
            EXPECT_EQ(_callCount, 0);
        }

        TEST(TimeSyncClientTest, SetEmptyNotifierFails)
        {
            TimeSyncClient _client;
            auto _result = _client.SetStateChangeNotifier(nullptr);
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
