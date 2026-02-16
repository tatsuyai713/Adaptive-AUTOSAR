#include <gtest/gtest.h>
#include "../../../src/ara/tsync/ntp_time_base_provider.h"

namespace ara
{
    namespace tsync
    {
        /// @brief Test subclass that overrides RunCommand for unit testing.
        class MockNtpProvider : public NtpTimeBaseProvider
        {
        public:
            MockNtpProvider()
                : NtpTimeBaseProvider(NtpDaemon::kChrony)
            {
            }

            explicit MockNtpProvider(NtpDaemon daemon)
                : NtpTimeBaseProvider(daemon)
            {
            }

            void SetMockOutput(const std::string &output)
            {
                mMockOutput = output;
                mMockAvailable = true;
            }

            void SetMockError()
            {
                mMockAvailable = false;
            }

        protected:
            core::Result<std::string> RunCommand(
                const std::string &) const override
            {
                if (!mMockAvailable)
                {
                    return core::Result<std::string>::FromError(
                        MakeErrorCode(TsyncErrc::kQueryFailed));
                }
                return core::Result<std::string>::FromValue(mMockOutput);
            }

        private:
            std::string mMockOutput;
            bool mMockAvailable{false};
        };

        TEST(NtpTimeBaseProviderTest, ProviderNameIsCorrect)
        {
            MockNtpProvider _provider;
            EXPECT_STREQ(_provider.GetProviderName(), "NTP");
        }

        TEST(NtpTimeBaseProviderTest, ParseChronyOutputValid)
        {
            // Sample chronyc -c tracking output (field 4 is offset in seconds)
            const std::string _output =
                "D8EF2300,216.239.35.0,2,1708000000.123456,"
                "-0.000001234,0.000005678,0.000001000,0.0,0.0,0.0,0.0,0.0,0.0";

            auto _result = NtpTimeBaseProvider::ParseChronyOutput(_output);
            ASSERT_TRUE(_result.HasValue());

            // -0.000001234 seconds = -1234 nanoseconds
            const auto _ns = _result.Value().count();
            EXPECT_TRUE(_ns >= -1300 && _ns <= -1100);
        }

        TEST(NtpTimeBaseProviderTest, ParseChronyOutputInvalid)
        {
            auto _result =
                NtpTimeBaseProvider::ParseChronyOutput("garbage,data");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NtpTimeBaseProviderTest, ParseNtpqOutputValid)
        {
            // Sample ntpq -c rv 0 output
            const std::string _output =
                "assid=0 status=0618 leap_none, sync_ntp, 1 event, "
                "leap_armed,\nversion=\"ntpq 4.2.8\", processor=\"x86_64\",\n"
                "system=\"Linux\", offset=1.500,\n"
                "sys_jitter=0.001, clk_jitter=0.002";

            auto _result = NtpTimeBaseProvider::ParseNtpqOutput(_output);
            ASSERT_TRUE(_result.HasValue());

            // offset=1.500 milliseconds = 1500000 nanoseconds
            const auto _ns = _result.Value().count();
            EXPECT_TRUE(_ns >= 1499000 && _ns <= 1501000);
        }

        TEST(NtpTimeBaseProviderTest, ParseNtpqOutputInvalid)
        {
            auto _result =
                NtpTimeBaseProvider::ParseNtpqOutput("no offset here");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NtpTimeBaseProviderTest, MockChronyUpdateSynchronizesClient)
        {
            MockNtpProvider _provider{NtpDaemon::kChrony};
            _provider.SetMockOutput(
                "D8EF2300,216.239.35.0,2,1708000000.123,"
                "-0.000005000,0.000001,0.0,0.0,0.0,0.0,0.0,0.0,0.0");

            TimeSyncClient _client;
            auto _result = _provider.UpdateTimeBase(_client);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_client.GetState(), SynchronizationState::kSynchronized);
        }

        TEST(NtpTimeBaseProviderTest, MockErrorMakesSourceUnavailable)
        {
            MockNtpProvider _provider{NtpDaemon::kAuto};
            _provider.SetMockError();

            EXPECT_FALSE(_provider.IsSourceAvailable());
        }
    }
}
