#include <gtest/gtest.h>
#include "../../../src/ara/tsync/ptp_time_base_provider.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Test subclass that overrides ReadPtpClock for unit testing.
        class MockPtpProvider : public PtpTimeBaseProvider
        {
        public:
            MockPtpProvider()
                : PtpTimeBaseProvider("/dev/null_ptp_test_nonexistent")
            {
            }

            void SetMockOffset(std::chrono::nanoseconds offset)
            {
                mMockOffset = offset;
                mMockAvailable = true;
            }

            void SetMockError()
            {
                mMockAvailable = false;
            }

        protected:
            core::Result<std::chrono::nanoseconds> ReadPtpClock() const override
            {
                if (!mMockAvailable)
                {
                    return core::Result<std::chrono::nanoseconds>::FromError(
                        MakeErrorCode(TsyncErrc::kQueryFailed));
                }
                return core::Result<std::chrono::nanoseconds>::FromValue(
                    mMockOffset);
            }

        private:
            std::chrono::nanoseconds mMockOffset{0};
            bool mMockAvailable{false};
        };

        TEST(PtpTimeBaseProviderTest, ProviderNameIsCorrect)
        {
            MockPtpProvider _provider;
            EXPECT_STREQ(_provider.GetProviderName(), "PTP/gPTP");
        }

        TEST(PtpTimeBaseProviderTest, DevicePathAccessor)
        {
            PtpTimeBaseProvider _provider{"/dev/ptp1"};
            EXPECT_EQ(_provider.GetDevicePath(), "/dev/ptp1");
        }

        TEST(PtpTimeBaseProviderTest, MockUpdateTimeBaseSynchronizesClient)
        {
            MockPtpProvider _provider;
            _provider.SetMockOffset(std::chrono::nanoseconds{5000000});

            TimeSyncClient _client;
            auto _result = _provider.UpdateTimeBase(_client);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_client.GetState(), SynchronizationState::kSynchronized);
        }

        TEST(PtpTimeBaseProviderTest, MockErrorCausesUpdateFailure)
        {
            MockPtpProvider _provider;
            _provider.SetMockError();

            TimeSyncClient _client;
            auto _result = _provider.UpdateTimeBase(_client);
            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Tsync");
        }

        TEST(PtpTimeBaseProviderTest, NonexistentDeviceIsUnavailable)
        {
            PtpTimeBaseProvider _provider{
                "/dev/ptp_nonexistent_test_device_12345"};
            EXPECT_FALSE(_provider.IsSourceAvailable());
        }
    }
}
