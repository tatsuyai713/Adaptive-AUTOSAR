#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../../../src/ara/tsync/time_sync_server.h"

namespace ara
{
    namespace tsync
    {
        // Stub provider that always succeeds by updating the client directly
        class StubTimeBaseProvider : public SynchronizedTimeBaseProvider
        {
        public:
            core::Result<void> UpdateTimeBase(TimeSyncClient &client) override
            {
                if (!mAvailable)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(TsyncErrc::kProviderUnavailable));
                }
                client.UpdateReferenceTime(
                    std::chrono::system_clock::now(),
                    std::chrono::steady_clock::now());
                return core::Result<void>::FromValue();
            }

            bool IsSourceAvailable() const override
            {
                return mAvailable;
            }

            const char *GetProviderName() const noexcept override
            {
                return "StubProvider";
            }

            bool mAvailable{true};
        };

        TEST(TimeSyncServerTest, ConstructAndDestroy)
        {
            StubTimeBaseProvider _provider;
            {
                TimeSyncServer _server{_provider};
            }
            // No crash on destruction
        }

        TEST(TimeSyncServerTest, StartAndStop)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider, {50U, 3U}};

            auto _result = _server.Start();
            EXPECT_TRUE(_result.HasValue());

            std::this_thread::sleep_for(std::chrono::milliseconds(120));

            _server.Stop();
        }

        TEST(TimeSyncServerTest, DoubleStartFails)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider, {50U, 3U}};

            _server.Start();
            auto _result = _server.Start();
            EXPECT_FALSE(_result.HasValue());

            _server.Stop();
        }

        TEST(TimeSyncServerTest, RegisterConsumer)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider, {50U, 5U}};

            TimeSyncClient _client;
            auto _result = _server.RegisterConsumer(_client);
            EXPECT_TRUE(_result.HasValue());

            _server.Start();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            _server.Stop();

            // After polling, the consumer should be synchronized
            EXPECT_EQ(_client.GetState(), SynchronizationState::kSynchronized);
        }

        TEST(TimeSyncServerTest, GetCurrentTimeAfterStart)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider, {50U, 5U}};

            _server.Start();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            auto _time = _server.GetCurrentTime();
            EXPECT_TRUE(_time.HasValue());

            _server.Stop();
        }

        TEST(TimeSyncServerTest, ProviderUnavailable)
        {
            StubTimeBaseProvider _provider;
            _provider.mAvailable = false;

            TimeSyncServer _server{_provider, {50U, 2U}};
            EXPECT_FALSE(_server.IsProviderAvailable());
        }

        TEST(TimeSyncServerTest, PollIntervalGetSet)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider};

            EXPECT_EQ(_server.GetPollIntervalMs(), 100U);

            _server.SetPollIntervalMs(200U);
            EXPECT_EQ(_server.GetPollIntervalMs(), 200U);

            // Minimum clamped to 10
            _server.SetPollIntervalMs(5U);
            EXPECT_GE(_server.GetPollIntervalMs(), 10U);
        }

        TEST(TimeSyncServerTest, AvailabilityCallback)
        {
            StubTimeBaseProvider _provider;
            TimeSyncServer _server{_provider, {50U, 5U}};

            bool _callbackCalled = false;
            _server.SetAvailabilityCallback(
                [&](bool avail)
                {
                    if (avail)
                    {
                        _callbackCalled = true;
                    }
                });

            _server.Start();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            _server.Stop();

            EXPECT_TRUE(_callbackCalled);
        }
    }
}
