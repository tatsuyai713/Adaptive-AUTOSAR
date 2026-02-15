#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

bool running;
AsyncBsdSocketLib::Poller poller;
application::platform::ExecutionManagement *executionManagement;

void performPolling()
{
    const std::chrono::milliseconds cSleepDuration{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (running)
    {
        poller.TryPoll();
        std::this_thread::sleep_for(cSleepDuration);
    }
}

int main(int argc, char *argv[])
{
    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    std::getchar();
    std::getchar();

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}