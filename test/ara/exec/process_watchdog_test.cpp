#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "../../../src/ara/exec/helper/process_watchdog.h"

namespace ara
{
    namespace exec
    {
        namespace helper
        {
            TEST(ProcessWatchdogTest, BasicProperties)
            {
                ProcessWatchdog wd("myProcess", std::chrono::milliseconds(500));
                EXPECT_EQ(wd.GetProcessName(), "myProcess");
                EXPECT_EQ(wd.GetTimeout(), std::chrono::milliseconds(500));
                EXPECT_FALSE(wd.IsRunning());
                EXPECT_FALSE(wd.IsExpired());
            }

            TEST(ProcessWatchdogTest, StartAndStop)
            {
                ProcessWatchdog wd("proc1", std::chrono::milliseconds(1000));
                EXPECT_FALSE(wd.IsRunning());

                wd.Start();
                EXPECT_TRUE(wd.IsRunning());

                wd.Stop();
                EXPECT_FALSE(wd.IsRunning());
            }

            TEST(ProcessWatchdogTest, ReportAliveExtendsDeadline)
            {
                ProcessWatchdog wd("proc2", std::chrono::milliseconds(100));
                wd.Start();

                // Report alive multiple times over a period longer than timeout
                for (int i = 0; i < 5; ++i)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    wd.ReportAlive();
                }

                EXPECT_FALSE(wd.IsExpired());
                wd.Stop();
            }

            TEST(ProcessWatchdogTest, ExpiresAfterTimeout)
            {
                ProcessWatchdog wd("proc3", std::chrono::milliseconds(50));
                wd.Start();

                // Wait long enough for timeout
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                EXPECT_TRUE(wd.IsExpired());
                wd.Stop();
            }

            TEST(ProcessWatchdogTest, ExpiryCallbackInvoked)
            {
                std::atomic<bool> callbackCalled{false};
                std::string capturedName;

                ProcessWatchdog wd(
                    "proc4",
                    std::chrono::milliseconds(50),
                    [&callbackCalled, &capturedName](const std::string &name)
                    {
                        capturedName = name;
                        callbackCalled.store(true);
                    });

                wd.Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                EXPECT_TRUE(callbackCalled.load());
                EXPECT_EQ(capturedName, "proc4");
                wd.Stop();
            }

            TEST(ProcessWatchdogTest, ReportAliveAfterStopIsSafe)
            {
                ProcessWatchdog wd("proc5", std::chrono::milliseconds(100));
                wd.Start();
                wd.Stop();

                // Should not crash or throw
                wd.ReportAlive();
                EXPECT_FALSE(wd.IsRunning());
            }

            TEST(ProcessWatchdogTest, MultipleStartCallsSafe)
            {
                ProcessWatchdog wd("proc6", std::chrono::milliseconds(500));
                wd.Start();
                wd.Start(); // Second start should be a no-op
                EXPECT_TRUE(wd.IsRunning());

                wd.Stop();
                EXPECT_FALSE(wd.IsRunning());
            }

            TEST(ProcessWatchdogTest, DestructorStopsAutomatically)
            {
                auto wd = new ProcessWatchdog("proc7", std::chrono::milliseconds(500));
                wd->Start();
                EXPECT_TRUE(wd->IsRunning());

                // Destructor should cleanly stop the thread
                delete wd;
                // If we get here without hanging, the test passes
            }

            TEST(ProcessWatchdogTest, StartupGraceDelaysFirstExpiry)
            {
                ProcessWatchdog::WatchdogOptions options;
                options.startupGrace = std::chrono::milliseconds(120);

                ProcessWatchdog wd(
                    "proc8",
                    std::chrono::milliseconds(60),
                    nullptr,
                    options);
                wd.Start();

                std::this_thread::sleep_for(std::chrono::milliseconds(90));
                EXPECT_FALSE(wd.IsExpired());

                std::this_thread::sleep_for(std::chrono::milliseconds(140));
                EXPECT_TRUE(wd.IsExpired());
                EXPECT_GE(wd.GetExpiryCount(), 1U);
                wd.Stop();
            }

            TEST(ProcessWatchdogTest, KeepRunningOnExpiryInvokesCallbackMultipleTimes)
            {
                std::atomic<uint32_t> callbackCount{0U};
                ProcessWatchdog::WatchdogOptions options;
                options.keepRunningOnExpiry = true;

                ProcessWatchdog wd(
                    "proc9",
                    std::chrono::milliseconds(40),
                    [&callbackCount](const std::string &)
                    {
                        callbackCount.fetch_add(1U);
                    },
                    options);

                wd.Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(220));
                wd.Stop();

                EXPECT_GE(wd.GetExpiryCount(), 2U);
                EXPECT_GE(callbackCount.load(), 2U);
            }

            TEST(ProcessWatchdogTest, CooldownSuppressesFrequentCallbacks)
            {
                std::atomic<uint32_t> callbackCount{0U};
                ProcessWatchdog::WatchdogOptions options;
                options.keepRunningOnExpiry = true;
                options.expiryCallbackCooldown = std::chrono::milliseconds(130);

                ProcessWatchdog wd(
                    "proc10",
                    std::chrono::milliseconds(35),
                    [&callbackCount](const std::string &)
                    {
                        callbackCount.fetch_add(1U);
                    },
                    options);

                wd.Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(280));
                wd.Stop();

                EXPECT_GE(wd.GetExpiryCount(), 2U);
                EXPECT_LE(callbackCount.load(), wd.GetExpiryCount());
                EXPECT_LE(callbackCount.load(), 3U);
            }

            TEST(ProcessWatchdogTest, ResetClearsExpiredAndAppliesGrace)
            {
                ProcessWatchdog::WatchdogOptions options;
                options.startupGrace = std::chrono::milliseconds(80);

                ProcessWatchdog wd(
                    "proc11",
                    std::chrono::milliseconds(50),
                    nullptr,
                    options);

                wd.Start();
                for (int retry = 0; retry < 15 && !wd.IsExpired(); ++retry)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                EXPECT_TRUE(wd.IsExpired());

                wd.Reset();
                EXPECT_FALSE(wd.IsExpired());

                std::this_thread::sleep_for(std::chrono::milliseconds(90));
                EXPECT_FALSE(wd.IsExpired());

                for (int retry = 0; retry < 10 && !wd.IsExpired(); ++retry)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                EXPECT_TRUE(wd.IsExpired());
                wd.Stop();
            }
        }
    }
}
