#include <gtest/gtest.h>
#include <csignal>
#include <thread>
#include <chrono>
#include "../../../src/ara/exec/signal_handler.h"

namespace ara
{
    namespace exec
    {
        class SignalHandlerTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                SignalHandler::Reset();
                SignalHandler::Register();
            }

            void TearDown() override
            {
                SignalHandler::Reset();
            }
        };

        TEST_F(SignalHandlerTest, InitialStateNotRequested)
        {
            EXPECT_FALSE(SignalHandler::IsTerminationRequested());
        }

        TEST_F(SignalHandlerTest, SigtermSetsFlag)
        {
            std::raise(SIGTERM);
            EXPECT_TRUE(SignalHandler::IsTerminationRequested());
        }

        TEST_F(SignalHandlerTest, SigintSetsFlag)
        {
            std::raise(SIGINT);
            EXPECT_TRUE(SignalHandler::IsTerminationRequested());
        }

        TEST_F(SignalHandlerTest, ResetClearsFlag)
        {
            std::raise(SIGTERM);
            EXPECT_TRUE(SignalHandler::IsTerminationRequested());
            SignalHandler::Reset();
            EXPECT_FALSE(SignalHandler::IsTerminationRequested());
        }

        TEST_F(SignalHandlerTest, WaitForTerminationReturnsAfterSignal)
        {
            // Pre-set the flag so WaitForTermination returns immediately
            std::raise(SIGTERM);

            // This should return immediately since the flag is already set
            SignalHandler::WaitForTermination();
            EXPECT_TRUE(SignalHandler::IsTerminationRequested());
        }

        TEST_F(SignalHandlerTest, WaitForTerminationBlocksUntilSignal)
        {
            std::atomic<bool> waited{false};

            std::thread waiter([&waited]()
                               {
                SignalHandler::WaitForTermination();
                waited.store(true); });

            // Give the thread time to start waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            EXPECT_FALSE(waited.load());

            // Send signal to unblock
            std::raise(SIGTERM);

            waiter.join();
            EXPECT_TRUE(waited.load());
            EXPECT_TRUE(SignalHandler::IsTerminationRequested());
        }
    }
}
