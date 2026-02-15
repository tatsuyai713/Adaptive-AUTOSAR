#include "./signal_handler.h"
#include <csignal>

namespace ara
{
    namespace exec
    {
        std::atomic<bool> SignalHandler::mTerminationRequested{false};
        std::mutex SignalHandler::mMutex;
        std::condition_variable SignalHandler::mCondVar;

        void SignalHandler::handleSignal(int signal)
        {
            (void)signal;
            mTerminationRequested.store(true);
            mCondVar.notify_all();
        }

        void SignalHandler::Register()
        {
            std::signal(SIGTERM, &SignalHandler::handleSignal);
            std::signal(SIGINT, &SignalHandler::handleSignal);
        }

        void SignalHandler::WaitForTermination()
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondVar.wait(lock, []()
                          { return mTerminationRequested.load(); });
        }

        bool SignalHandler::IsTerminationRequested() noexcept
        {
            return mTerminationRequested.load();
        }

        void SignalHandler::Reset() noexcept
        {
            mTerminationRequested.store(false);
        }
    }
}
