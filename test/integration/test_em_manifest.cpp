/// @file test/integration/test_em_manifest.cpp
/// @brief Integration test: ManifestLoader → ExecutionManager → process launch
///
/// Verifies the end-to-end ARXML manifest loading, process registration,
/// function group activation, and process lifecycle management.
/// Uses /bin/sleep as a lightweight child process.

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ara/exec/manifest_loader.h"
#include "ara/exec/execution_manager.h"
#include "ara/exec/execution_server.h"
#include "ara/exec/state_server.h"
#include "ara/com/someip/rpc/rpc_server.h"

/// Minimal mock RPC server for EM construction.
class TestRpcServer : public ara::com::someip::rpc::RpcServer
{
public:
    TestRpcServer() : RpcServer(1, 1) {}
};

static int gFailures = 0;

#define CHECK(cond, msg)                                           \
    do {                                                           \
        if (!(cond)) {                                             \
            std::cerr << "[FAIL] " << msg << "\n";                 \
            ++gFailures;                                           \
        } else {                                                   \
            std::cout << "[PASS] " << msg << "\n";                 \
        }                                                          \
    } while (false)

/// Write a temporary ARXML manifest file.
static std::string WriteTempManifest()
{
    const std::string path{"/tmp/autosar_em_test_manifest.arxml"};
    std::ofstream ofs(path);
    ofs << R"(<?xml version="1.0" encoding="UTF-8"?>
<AUTOSAR xmlns="http://autosar.org/schema/r4.0">
  <AR-PACKAGES>
    <AR-PACKAGE>
      <SHORT-NAME>TestManifest</SHORT-NAME>
      <ELEMENTS>

        <PROCESS-DESIGN>
          <SHORT-NAME>SleepAppA</SHORT-NAME>
          <EXECUTABLE-REF>/bin/sleep</EXECUTABLE-REF>
          <FUNCTION-GROUP-REF>TestFG</FUNCTION-GROUP-REF>
          <STATE-REF>Running</STATE-REF>
          <STARTUP-PRIORITY>10</STARTUP-PRIORITY>
          <STARTUP-GRACE-MS>2000</STARTUP-GRACE-MS>
          <TERMINATION-TIMEOUT-MS>3000</TERMINATION-TIMEOUT-MS>
        </PROCESS-DESIGN>

        <PROCESS-DESIGN>
          <SHORT-NAME>SleepAppB</SHORT-NAME>
          <EXECUTABLE-REF>/bin/sleep</EXECUTABLE-REF>
          <FUNCTION-GROUP-REF>TestFG</FUNCTION-GROUP-REF>
          <STATE-REF>Running</STATE-REF>
          <STARTUP-PRIORITY>20</STARTUP-PRIORITY>
          <STARTUP-GRACE-MS>2000</STARTUP-GRACE-MS>
          <TERMINATION-TIMEOUT-MS>3000</TERMINATION-TIMEOUT-MS>
          <DEPENDS-ON>SleepAppA</DEPENDS-ON>
        </PROCESS-DESIGN>

        <PROCESS-DESIGN>
          <SHORT-NAME>DiagModeApp</SHORT-NAME>
          <EXECUTABLE-REF>/bin/sleep</EXECUTABLE-REF>
          <FUNCTION-GROUP-REF>TestFG</FUNCTION-GROUP-REF>
          <STATE-REF>Diagnostic</STATE-REF>
          <STARTUP-PRIORITY>10</STARTUP-PRIORITY>
          <STARTUP-GRACE-MS>2000</STARTUP-GRACE-MS>
          <TERMINATION-TIMEOUT-MS>3000</TERMINATION-TIMEOUT-MS>
        </PROCESS-DESIGN>

      </ELEMENTS>
    </AR-PACKAGE>
  </AR-PACKAGES>
</AUTOSAR>
)";
    ofs.close();
    return path;
}

int main()
{
    std::cout << "=== ARXML Manifest + Execution Manager Integration Test ===\n\n";

    // -------------------------------------------------------------------
    // Test 1: ManifestLoader - Load from file
    // -------------------------------------------------------------------
    std::cout << "--- Test 1: ManifestLoader - Load from file ---\n";
    const std::string manifestPath{WriteTempManifest()};

    ara::exec::ExecutionManifestLoader loader;
    auto result{loader.LoadFromFile(manifestPath)};

    CHECK(result.Success, "LoadFromFile succeeds");
    CHECK(result.Entries.size() == 3U, "3 entries loaded (got " +
          std::to_string(result.Entries.size()) + ")");

    if (result.Entries.size() >= 3U)
    {
        CHECK(result.Entries[0].Descriptor.name == "SleepAppA",
              "First entry is SleepAppA");
        CHECK(result.Entries[1].Descriptor.name == "SleepAppB",
              "Second entry is SleepAppB");
        CHECK(result.Entries[2].Descriptor.name == "DiagModeApp",
              "Third entry is DiagModeApp");

        CHECK(result.Entries[0].Descriptor.executable == "/bin/sleep",
              "Executable is /bin/sleep");
        CHECK(result.Entries[0].Descriptor.functionGroup == "TestFG",
              "Function group is TestFG");
        CHECK(result.Entries[0].Descriptor.activeState == "Running",
              "Active state is Running");
        CHECK(result.Entries[0].StartupPriority == 10U,
              "Startup priority is 10");

        CHECK(result.Entries[1].Dependencies.size() == 1U &&
              result.Entries[1].Dependencies[0] == "SleepAppA",
              "SleepAppB depends on SleepAppA");
    }

    // -------------------------------------------------------------------
    // Test 2: Validation (no duplicates, no cycles)
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 2: Validation ---\n";
    auto warnings{loader.ValidateEntries(result.Entries)};
    CHECK(warnings.empty(), "No validation warnings (got " +
          std::to_string(warnings.size()) + ")");
    for (const auto &w : warnings)
    {
        std::cerr << "  Warning: " << w << "\n";
    }

    // -------------------------------------------------------------------
    // Test 3: Topological sort
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 3: Topological sort ---\n";
    auto sorted{loader.SortByStartupOrder(result.Entries)};
    CHECK(sorted.size() == 3U, "Sorted has 3 entries");

    if (sorted.size() >= 2U)
    {
        // SleepAppA must come before SleepAppB (dependency)
        std::size_t idxA{0U}, idxB{0U};
        for (std::size_t i{0U}; i < sorted.size(); ++i)
        {
            if (sorted[i].Descriptor.name == "SleepAppA") idxA = i;
            if (sorted[i].Descriptor.name == "SleepAppB") idxB = i;
        }
        CHECK(idxA < idxB, "SleepAppA before SleepAppB in startup order");
    }

    // -------------------------------------------------------------------
    // Test 4: ExecutionManager - Register from manifest
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 4: ExecutionManager - Register from manifest ---\n";

    // Build EM with mock RPC servers and a function group that has
    // "Running" and "Diagnostic" states.
    TestRpcServer execRpc;
    TestRpcServer stateRpc;
    ara::exec::ExecutionServer execServer{&execRpc};

    std::set<std::pair<std::string, std::string>> fgStates{
        {"TestFG", "Off"},
        {"TestFG", "Running"},
        {"TestFG", "Diagnostic"}};
    std::map<std::string, std::string> fgInit{{"TestFG", "Off"}};
    ara::exec::StateServer stateServer{&stateRpc, std::move(fgStates), std::move(fgInit)};

    ara::exec::ExecutionManager em{execServer, stateServer};

    for (const auto &entry : sorted)
    {
        // Add "60" as argument to /bin/sleep so it sleeps 60s
        auto desc{entry.Descriptor};
        desc.arguments.push_back("60");
        auto regResult{em.RegisterProcess(desc)};
        CHECK(regResult.HasValue(),
              "Register " + desc.name + " succeeded");
    }

    auto allStatuses{em.GetAllProcessStatuses()};
    CHECK(allStatuses.size() == 3U, "3 processes registered");

    for (const auto &status : allStatuses)
    {
        CHECK(status.managedState ==
              ara::exec::ManagedProcessState::kNotRunning,
              status.descriptor.name + " is kNotRunning");
    }

    // -------------------------------------------------------------------
    // Test 5: Start EM and activate function group
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 5: ActivateFunctionGroup(TestFG, Running) ---\n";

    std::vector<std::string> stateChanges;
    em.SetProcessStateChangeHandler(
        [&stateChanges](const std::string &name,
                        ara::exec::ManagedProcessState state)
        {
            stateChanges.push_back(
                name + " -> " + std::to_string(static_cast<int>(state)));
        });

    auto startResult{em.Start()};
    CHECK(startResult.HasValue(), "EM.Start() succeeded");

    // Activate "Running" state — should launch SleepAppA and SleepAppB
    // (DiagModeApp is in "Diagnostic" state, so should NOT be launched)
    auto activateResult{em.ActivateFunctionGroup("TestFG", "Running")};
    CHECK(activateResult.HasValue(), "ActivateFG(TestFG, Running) succeeded");

    // Give processes time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    allStatuses = em.GetAllProcessStatuses();
    int runningCount{0};
    int notRunningCount{0};
    for (const auto &status : allStatuses)
    {
        std::cout << "  " << status.descriptor.name
                  << ": state=" << static_cast<int>(status.managedState)
                  << " pid=" << status.pid << "\n";

        if (status.managedState == ara::exec::ManagedProcessState::kStarting ||
            status.managedState == ara::exec::ManagedProcessState::kRunning)
        {
            ++runningCount;
            CHECK(status.pid > 0,
                  status.descriptor.name + " has valid PID");
        }
        else
        {
            ++notRunningCount;
        }
    }

    CHECK(runningCount == 2, "2 processes launched (Running state)");
    CHECK(notRunningCount == 1, "1 process NOT launched (Diagnostic state)");

    // Verify DiagModeApp specifically was not launched
    auto diagStatus{em.GetProcessStatus("DiagModeApp")};
    CHECK(diagStatus.HasValue() &&
          diagStatus.Value().managedState ==
              ara::exec::ManagedProcessState::kNotRunning,
          "DiagModeApp remains kNotRunning (wrong FG state)");

    // -------------------------------------------------------------------
    // Test 6: State transition — Running → Diagnostic
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 6: State transition (Running -> Diagnostic) ---\n";

    auto transResult{em.ActivateFunctionGroup("TestFG", "Diagnostic")};
    CHECK(transResult.HasValue(),
          "ActivateFG(TestFG, Diagnostic) succeeded");

    // Wait for termination + launch
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    allStatuses = em.GetAllProcessStatuses();
    int diagRunning{0};
    for (const auto &status : allStatuses)
    {
        std::cout << "  " << status.descriptor.name
                  << ": state=" << static_cast<int>(status.managedState)
                  << " pid=" << status.pid << "\n";

        if (status.descriptor.name == "DiagModeApp" &&
            (status.managedState == ara::exec::ManagedProcessState::kStarting ||
             status.managedState == ara::exec::ManagedProcessState::kRunning))
        {
            ++diagRunning;
        }
    }
    CHECK(diagRunning == 1, "DiagModeApp is now running");

    // SleepAppA and SleepAppB should be terminated
    auto sleepAStatus{em.GetProcessStatus("SleepAppA")};
    if (sleepAStatus.HasValue())
    {
        CHECK(sleepAStatus.Value().managedState ==
                  ara::exec::ManagedProcessState::kTerminated ||
              sleepAStatus.Value().managedState ==
                  ara::exec::ManagedProcessState::kNotRunning,
              "SleepAppA is terminated after FG transition");
    }

    // -------------------------------------------------------------------
    // Test 7: TerminateFunctionGroup
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 7: TerminateFunctionGroup ---\n";

    auto termResult{em.TerminateFunctionGroup("TestFG")};
    CHECK(termResult.HasValue(), "TerminateFG(TestFG) succeeded");

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    allStatuses = em.GetAllProcessStatuses();
    bool allStopped{true};
    for (const auto &status : allStatuses)
    {
        std::cout << "  " << status.descriptor.name
                  << ": state=" << static_cast<int>(status.managedState)
                  << " pid=" << status.pid << "\n";

        if (status.managedState == ara::exec::ManagedProcessState::kStarting ||
            status.managedState == ara::exec::ManagedProcessState::kRunning)
        {
            allStopped = false;
        }
    }
    CHECK(allStopped, "All processes stopped after TerminateFG");

    // -------------------------------------------------------------------
    // Test 8: State change handler was invoked
    // -------------------------------------------------------------------
    std::cout << "\n--- Test 8: State change handler ---\n";
    CHECK(!stateChanges.empty(), "State change handler was invoked ("
          + std::to_string(stateChanges.size()) + " changes)");
    for (const auto &change : stateChanges)
    {
        std::cout << "  StateChange: " << change << "\n";
    }

    // -------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------
    em.Stop();
    ::unlink(manifestPath.c_str());

    // -------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------
    std::cout << "\n=== Summary ===\n";
    if (gFailures == 0)
    {
        std::cout << "ALL TESTS PASSED\n";
    }
    else
    {
        std::cout << gFailures << " TEST(S) FAILED\n";
    }
    return gFailures;
}
