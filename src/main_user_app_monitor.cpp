/// @file src/main_user_app_monitor.cpp
/// @brief Resident daemon that monitors registered user applications.

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "./ara/core/instance_specifier.h"
#include "./ara/exec/execution_error_event.h"
#include "./ara/phm/restart_recovery_action.h"

namespace
{
    struct AppRegistration
    {
        std::string Name;
        pid_t Pid{0};
        std::string HeartbeatFile;
        std::uint32_t HeartbeatTimeoutMs{0U};
        std::string InstanceSpecifier;
        std::uint32_t RestartLimit{3U};
        std::uint32_t RestartWindowMs{30000U};
        std::string RestartCommand;
    };

    struct AppStatus
    {
        AppRegistration Registration;
        bool Alive{false};
        bool ZombieDetected{false};
        bool HeartbeatChecked{false};
        bool HeartbeatFresh{true};
        bool PhmChecked{false};
        bool PhmFresh{true};
        bool PhmStatusHealthy{true};
        std::uint32_t PhmStatusCode{0U};
        bool StartupGraceApplied{false};
        bool DeactivatedStopAllowed{false};
        bool RecoveryTriggered{false};
        bool Restarted{false};
        bool RestartSuppressed{false};
        bool RestartBackoffActive{false};
    };

    struct MonitorSummary
    {
        std::size_t RegisteredApps{0U};
        std::size_t InvalidRows{0U};
        std::size_t AliveApps{0U};
        std::size_t ZombieApps{0U};
        std::size_t HealthyApps{0U};
        std::size_t UnhealthyApps{0U};
        std::size_t HeartbeatChecks{0U};
        std::size_t HeartbeatFailures{0U};
        std::size_t PhmChecks{0U};
        std::size_t PhmFailures{0U};
        std::size_t PhmDeactivatedApps{0U};
        std::size_t StartupGraceApps{0U};
        std::size_t RestartAttempts{0U};
        std::size_t RestartSuccesses{0U};
        std::size_t RestartSuppressed{0U};
        std::size_t RestartBackoffSuppressions{0U};
        std::size_t KilledApps{0U};
    };

    struct RestartRuntimeState
    {
        std::deque<std::uint64_t> AttemptEpochMs;
        pid_t LastSeenPid{0};
        std::uint64_t LastPidChangeEpochMs{0U};
    };

    struct PhmStatusSample
    {
        bool Valid{false};
        std::uint32_t StatusCode{0U};
        std::uint64_t UpdatedEpochMs{0U};
    };

    std::atomic_bool gRunning{true};
    std::map<std::string, RestartRuntimeState> gRestartState;

    void RequestStop(int) noexcept
    {
        gRunning = false;
    }

    std::string GetEnvOrDefault(const char *key, std::string fallback)
    {
        const char *value{std::getenv(key)};
        if (value != nullptr)
        {
            return value;
        }

        return fallback;
    }

    std::uint32_t GetEnvU32(const char *key, std::uint32_t fallback, std::uint32_t maxValue)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(value)};
            if (parsed > maxValue)
            {
                return fallback;
            }
            return static_cast<std::uint32_t>(parsed);
        }
        catch (...)
        {
            return fallback;
        }
    }

    bool GetEnvBool(const char *key, bool fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        const std::string text{value};
        if (text == "1" || text == "true" || text == "TRUE" || text == "on")
        {
            return true;
        }
        if (text == "0" || text == "false" || text == "FALSE" || text == "off")
        {
            return false;
        }

        return fallback;
    }

    int ResolveKillSignal()
    {
        const std::string signalText{
            GetEnvOrDefault("AUTOSAR_USER_APP_MONITOR_KILL_SIGNAL", "TERM")};

        if (signalText == "TERM")
        {
            return SIGTERM;
        }
        if (signalText == "KILL")
        {
            return SIGKILL;
        }
        if (signalText == "INT")
        {
            return SIGINT;
        }
        if (signalText == "HUP")
        {
            return SIGHUP;
        }

        try
        {
            const int parsed{std::stoi(signalText)};
            if (parsed > 0 && parsed < 65)
            {
                return parsed;
            }
        }
        catch (...)
        {
        }

        return SIGTERM;
    }

    void Trim(std::string &value)
    {
        value.erase(
            value.begin(),
            std::find_if(
                value.begin(),
                value.end(),
                [](int ch)
                { return !std::isspace(ch); }));
        value.erase(
            std::find_if(
                value.rbegin(),
                value.rend(),
                [](int ch)
                { return !std::isspace(ch); })
                .base(),
            value.end());
    }

    bool ParsePid(const std::string &text, pid_t &pid)
    {
        try
        {
            const long parsed{std::stol(text)};
            if (parsed <= 1)
            {
                return false;
            }

            pid = static_cast<pid_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseTimeout(const std::string &text, std::uint32_t &timeoutMs)
    {
        try
        {
            const std::uint64_t parsed{std::stoull(text)};
            if (parsed > 24ULL * 3600ULL * 1000ULL)
            {
                return false;
            }

            timeoutMs = static_cast<std::uint32_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseOptionalU32(
        const std::string &text,
        std::uint32_t fallback,
        std::uint32_t maxValue,
        std::uint32_t &value)
    {
        if (text.empty())
        {
            value = fallback;
            return true;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(text)};
            if (parsed > maxValue)
            {
                return false;
            }
            value = static_cast<std::uint32_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string SanitizeToken(const std::string &value)
    {
        std::string sanitized;
        sanitized.reserve(value.size());

        for (const char ch : value)
        {
            const bool alphaNumeric{
                std::isalnum(static_cast<unsigned char>(ch)) != 0};
            if (alphaNumeric || ch == '_' || ch == '-' || ch == '.')
            {
                sanitized.push_back(ch);
            }
            else
            {
                sanitized.push_back('_');
            }
        }

        if (sanitized.empty())
        {
            sanitized = "app";
        }

        return sanitized;
    }

    std::string BuildRuntimeStateKey(const AppRegistration &registration)
    {
        const std::string instance{
            registration.InstanceSpecifier.empty()
                ? registration.Name
                : registration.InstanceSpecifier};
        return instance + "|" + registration.Name;
    }

    bool IsWithinStartupGrace(
        const RestartRuntimeState &runtimeState,
        std::uint64_t nowEpochMs,
        std::uint32_t startupGraceMs)
    {
        if (startupGraceMs == 0U || runtimeState.LastPidChangeEpochMs == 0U)
        {
            return false;
        }

        if (nowEpochMs < runtimeState.LastPidChangeEpochMs)
        {
            return false;
        }

        return (nowEpochMs - runtimeState.LastPidChangeEpochMs) <=
               static_cast<std::uint64_t>(startupGraceMs);
    }

    void PruneRuntimeState(const std::vector<AppRegistration> &registrations)
    {
        std::set<std::string> activeKeys;
        for (const auto &registration : registrations)
        {
            activeKeys.insert(BuildRuntimeStateKey(registration));
        }

        for (auto iterator = gRestartState.begin();
             iterator != gRestartState.end();)
        {
            if (activeKeys.find(iterator->first) == activeKeys.end())
            {
                iterator = gRestartState.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }

    bool ParseCsvLine(const std::string &line, AppRegistration &registration)
    {
        std::stringstream stream(line);
        std::string name;
        std::string pidText;
        std::string heartbeatFile;
        std::string timeoutText;
        std::string instanceSpecifier;
        std::string restartLimitText;
        std::string restartWindowText;
        std::string restartCommand;

        if (!std::getline(stream, name, ',') ||
            !std::getline(stream, pidText, ',') ||
            !std::getline(stream, heartbeatFile, ',') ||
            !std::getline(stream, timeoutText, ','))
        {
            return false;
        }

        (void)std::getline(stream, instanceSpecifier, ',');
        (void)std::getline(stream, restartLimitText, ',');
        (void)std::getline(stream, restartWindowText, ',');
        (void)std::getline(stream, restartCommand);

        Trim(name);
        Trim(pidText);
        Trim(heartbeatFile);
        Trim(timeoutText);
        Trim(instanceSpecifier);
        Trim(restartLimitText);
        Trim(restartWindowText);
        Trim(restartCommand);

        if (name.empty())
        {
            return false;
        }

        pid_t pid{0};
        if (!ParsePid(pidText, pid))
        {
            return false;
        }

        std::uint32_t timeoutMs{0U};
        if (!ParseTimeout(timeoutText, timeoutMs))
        {
            return false;
        }

        std::uint32_t restartLimit{3U};
        if (!ParseOptionalU32(restartLimitText, 3U, 1000U, restartLimit))
        {
            return false;
        }

        std::uint32_t restartWindowMs{30000U};
        if (!ParseOptionalU32(
                restartWindowText,
                30000U,
                24U * 3600U * 1000U,
                restartWindowMs))
        {
            return false;
        }

        registration.Name = std::move(name);
        registration.Pid = pid;
        registration.HeartbeatFile = std::move(heartbeatFile);
        registration.HeartbeatTimeoutMs = timeoutMs;
        registration.InstanceSpecifier = instanceSpecifier.empty()
                                             ? registration.Name
                                             : instanceSpecifier;
        registration.RestartLimit = restartLimit;
        registration.RestartWindowMs = restartWindowMs;
        registration.RestartCommand = std::move(restartCommand);

        return true;
    }

    std::vector<AppRegistration> LoadRegistry(
        const std::string &registryFile,
        std::size_t &invalidRows)
    {
        invalidRows = 0U;
        std::vector<AppRegistration> registrations;

        std::ifstream stream(registryFile);
        if (!stream.is_open())
        {
            return registrations;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            Trim(line);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            AppRegistration registration;
            if (ParseCsvLine(line, registration))
            {
                registrations.emplace_back(std::move(registration));
            }
            else
            {
                ++invalidRows;
            }
        }

        return registrations;
    }

    bool WriteRegistry(
        const std::string &registryFile,
        const std::vector<AppRegistration> &registrations)
    {
        const std::string tempFile{registryFile + ".tmp"};
        std::ofstream stream(tempFile, std::ios::trunc);
        if (!stream.is_open())
        {
            return false;
        }

        stream << "# name,pid,heartbeat_file,heartbeat_timeout_ms,instance_specifier,restart_limit,restart_window_ms,restart_command\n";
        for (const auto &registration : registrations)
        {
            stream << registration.Name << ","
                   << registration.Pid << ","
                   << registration.HeartbeatFile << ","
                   << registration.HeartbeatTimeoutMs << ","
                   << registration.InstanceSpecifier << ","
                   << registration.RestartLimit << ","
                   << registration.RestartWindowMs << ","
                   << registration.RestartCommand << "\n";
        }

        stream.close();
        return ::rename(tempFile.c_str(), registryFile.c_str()) == 0;
    }

    char TryReadProcessState(pid_t pid)
    {
        const std::string statFile{
            "/proc/" + std::to_string(pid) + "/stat"};
        std::ifstream stream(statFile);
        if (!stream.is_open())
        {
            return '\0';
        }

        std::string line;
        std::getline(stream, line);
        const std::size_t closeParenPos{line.rfind(')')};
        if (closeParenPos == std::string::npos ||
            (closeParenPos + 2U) >= line.size())
        {
            return '\0';
        }

        return line.at(closeParenPos + 2U);
    }

    void TryReapChildProcess(pid_t pid)
    {
        int status{0};
        while (::waitpid(pid, &status, WNOHANG) > 0)
        {
        }
    }

    bool IsProcessAlive(pid_t pid, bool *zombieDetected = nullptr)
    {
        if (zombieDetected != nullptr)
        {
            *zombieDetected = false;
        }

        if (::kill(pid, 0) != 0 && errno != EPERM)
        {
            return false;
        }

        const char state{TryReadProcessState(pid)};
        if (state == 'Z')
        {
            if (zombieDetected != nullptr)
            {
                *zombieDetected = true;
            }
            TryReapChildProcess(pid);
            return false;
        }

        return true;
    }

    std::uint64_t NowEpochMs()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    bool IsHeartbeatFresh(
        const AppRegistration &registration,
        std::uint32_t heartbeatGraceMs,
        bool &checked)
    {
        checked = false;
        if (registration.HeartbeatFile.empty() ||
            registration.HeartbeatTimeoutMs == 0U)
        {
            return true;
        }

        checked = true;
        struct stat status
        {
        };
        if (::stat(registration.HeartbeatFile.c_str(), &status) != 0)
        {
            return false;
        }

        const std::uint64_t modifiedMs{
            static_cast<std::uint64_t>(status.st_mtime) * 1000ULL};
        const std::uint64_t deadlineMs{
            modifiedMs +
            static_cast<std::uint64_t>(registration.HeartbeatTimeoutMs) +
            static_cast<std::uint64_t>(heartbeatGraceMs)};

        return NowEpochMs() <= deadlineMs;
    }

    std::string ResolvePhmStatusFilePath(
        const std::string &runtimeRoot,
        const std::string &instanceSpecifier)
    {
        const std::string filename{SanitizeToken(instanceSpecifier) + ".status"};
        if (!runtimeRoot.empty() && runtimeRoot.back() == '/')
        {
            return runtimeRoot + filename;
        }

        return runtimeRoot + "/" + filename;
    }

    bool TryReadPhmStatus(
        const std::string &runtimeRoot,
        const AppRegistration &registration,
        PhmStatusSample &sample)
    {
        sample = PhmStatusSample{};

        if (registration.InstanceSpecifier.empty())
        {
            return false;
        }

        const std::string statusFile{
            ResolvePhmStatusFilePath(runtimeRoot, registration.InstanceSpecifier)};
        std::ifstream stream(statusFile);
        if (!stream.is_open())
        {
            return false;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            Trim(line);
            const std::size_t delimiter{line.find('=')};
            if (delimiter == std::string::npos)
            {
                continue;
            }

            std::string key{line.substr(0U, delimiter)};
            std::string value{line.substr(delimiter + 1U)};
            Trim(key);
            Trim(value);

            if (key == "status")
            {
                try
                {
                    sample.StatusCode = static_cast<std::uint32_t>(
                        std::stoul(value));
                }
                catch (...)
                {
                    return false;
                }
            }
            else if (key == "updated_epoch_ms")
            {
                try
                {
                    sample.UpdatedEpochMs = static_cast<std::uint64_t>(
                        std::stoull(value));
                }
                catch (...)
                {
                    return false;
                }
            }
        }

        sample.Valid = true;
        return true;
    }

    bool IsPhmStatusHealthy(
        const PhmStatusSample &sample,
        bool allowDeactivatedAsHealthy)
    {
        constexpr std::uint32_t cOk{0U};
        constexpr std::uint32_t cDeactivated{3U};

        if (sample.StatusCode == cOk)
        {
            return true;
        }
        if (allowDeactivatedAsHealthy &&
            sample.StatusCode == cDeactivated)
        {
            return true;
        }

        return false;
    }

    bool IsPhmStatusFresh(
        const AppRegistration &registration,
        const PhmStatusSample &sample,
        std::uint32_t heartbeatGraceMs)
    {
        if (registration.HeartbeatTimeoutMs == 0U)
        {
            return true;
        }

        if (sample.UpdatedEpochMs == 0U)
        {
            return false;
        }

        const std::uint64_t deadlineMs{
            sample.UpdatedEpochMs +
            static_cast<std::uint64_t>(registration.HeartbeatTimeoutMs) +
            static_cast<std::uint64_t>(heartbeatGraceMs)};
        return NowEpochMs() <= deadlineMs;
    }

    void EnsureDirectoryTree(const std::string &directoryPath)
    {
        if (directoryPath.empty())
        {
            return;
        }

        std::string normalized{directoryPath};
        if (normalized.back() == '/')
        {
            normalized.pop_back();
        }

        if (normalized.empty())
        {
            return;
        }

        std::string partialPath;
        if (normalized.front() == '/')
        {
            partialPath = "/";
        }

        std::size_t begin{0U};
        while (begin < normalized.size())
        {
            const std::size_t delimiter{normalized.find('/', begin)};
            const std::size_t length{
                (delimiter == std::string::npos)
                    ? normalized.size() - begin
                    : delimiter - begin};

            const std::string part{normalized.substr(begin, length)};
            if (!part.empty())
            {
                if (!partialPath.empty() && partialPath.back() != '/')
                {
                    partialPath.push_back('/');
                }
                partialPath += part;
                ::mkdir(partialPath.c_str(), 0755);
            }

            if (delimiter == std::string::npos)
            {
                break;
            }
            begin = delimiter + 1U;
        }
    }

    void EnsureDirectoryForFile(const std::string &filepath)
    {
        const auto delimiterPos{filepath.rfind('/')};
        if (delimiterPos == std::string::npos || delimiterPos == 0U)
        {
            return;
        }

        EnsureDirectoryTree(filepath.substr(0U, delimiterPos));
    }

    void WriteStatus(
        const std::string &statusFile,
        const MonitorSummary &summary,
        const std::vector<AppStatus> &appStatuses)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "registered_apps=" << summary.RegisteredApps << "\n";
        stream << "invalid_rows=" << summary.InvalidRows << "\n";
        stream << "alive_apps=" << summary.AliveApps << "\n";
        stream << "zombie_apps=" << summary.ZombieApps << "\n";
        stream << "healthy_apps=" << summary.HealthyApps << "\n";
        stream << "unhealthy_apps=" << summary.UnhealthyApps << "\n";
        stream << "heartbeat_checks=" << summary.HeartbeatChecks << "\n";
        stream << "heartbeat_failures=" << summary.HeartbeatFailures << "\n";
        stream << "phm_checks=" << summary.PhmChecks << "\n";
        stream << "phm_failures=" << summary.PhmFailures << "\n";
        stream << "phm_deactivated_apps=" << summary.PhmDeactivatedApps << "\n";
        stream << "startup_grace_apps=" << summary.StartupGraceApps << "\n";
        stream << "restart_attempts=" << summary.RestartAttempts << "\n";
        stream << "restart_successes=" << summary.RestartSuccesses << "\n";
        stream << "restart_suppressed=" << summary.RestartSuppressed << "\n";
        stream << "restart_backoff_suppressions="
               << summary.RestartBackoffSuppressions << "\n";
        stream << "killed_apps=" << summary.KilledApps << "\n";
        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";

        for (std::size_t index = 0U; index < appStatuses.size(); ++index)
        {
            const auto &status{appStatuses[index]};
            const bool healthy{
                status.DeactivatedStopAllowed ||
                (status.Alive &&
                 (!status.HeartbeatChecked || status.HeartbeatFresh) &&
                 (!status.PhmChecked || (status.PhmFresh && status.PhmStatusHealthy)))};

            stream << "app[" << index << "].name=" << status.Registration.Name << "\n";
            stream << "app[" << index << "].pid=" << status.Registration.Pid << "\n";
            stream << "app[" << index << "].alive=" << (status.Alive ? "true" : "false") << "\n";
            stream << "app[" << index << "].zombie_detected="
                   << (status.ZombieDetected ? "true" : "false") << "\n";
            stream << "app[" << index << "].heartbeat_checked="
                   << (status.HeartbeatChecked ? "true" : "false") << "\n";
            stream << "app[" << index << "].heartbeat_fresh="
                   << (status.HeartbeatFresh ? "true" : "false") << "\n";
            stream << "app[" << index << "].phm_checked="
                   << (status.PhmChecked ? "true" : "false") << "\n";
            stream << "app[" << index << "].phm_fresh="
                   << (status.PhmFresh ? "true" : "false") << "\n";
            stream << "app[" << index << "].phm_status_code="
                   << status.PhmStatusCode << "\n";
            stream << "app[" << index << "].phm_status_healthy="
                   << (status.PhmStatusHealthy ? "true" : "false") << "\n";
            stream << "app[" << index << "].startup_grace_applied="
                   << (status.StartupGraceApplied ? "true" : "false") << "\n";
            stream << "app[" << index << "].deactivated_stop_allowed="
                   << (status.DeactivatedStopAllowed ? "true" : "false") << "\n";
            stream << "app[" << index << "].recovery_triggered="
                   << (status.RecoveryTriggered ? "true" : "false") << "\n";
            stream << "app[" << index << "].restarted="
                   << (status.Restarted ? "true" : "false") << "\n";
            stream << "app[" << index << "].restart_suppressed="
                   << (status.RestartSuppressed ? "true" : "false") << "\n";
            stream << "app[" << index << "].restart_backoff_active="
                   << (status.RestartBackoffActive ? "true" : "false") << "\n";
            stream << "app[" << index << "].healthy=" << (healthy ? "true" : "false") << "\n";
        }
    }

    bool SpawnCommand(const std::string &command, pid_t &spawnedPid)
    {
        if (command.empty())
        {
            return false;
        }

        pid_t pid{::fork()};
        if (pid < 0)
        {
            return false;
        }

        if (pid == 0)
        {
            ::execl("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }

        spawnedPid = pid;
        return true;
    }

    void TryTerminateProcess(pid_t pid, int killSignal, std::size_t &killedCounter)
    {
        if (pid <= 1)
        {
            return;
        }

        if (!IsProcessAlive(pid))
        {
            return;
        }

        if (::kill(pid, killSignal) == 0)
        {
            ++killedCounter;
        }

        const std::uint32_t waitStepMs{50U};
        const std::uint32_t waitBudgetMs{1000U};
        std::uint32_t waited{0U};
        while (waited < waitBudgetMs)
        {
            if (!IsProcessAlive(pid))
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(waitStepMs));
            waited += waitStepMs;
        }

        if (IsProcessAlive(pid) && killSignal != SIGKILL)
        {
            if (::kill(pid, SIGKILL) == 0)
            {
                ++killedCounter;
            }
        }

        int status{0};
        (void)::waitpid(pid, &status, WNOHANG);
    }

    bool CanAttemptRestart(
        const AppRegistration &registration,
        std::uint64_t nowEpochMs,
        RestartRuntimeState &state)
    {
        if (registration.RestartLimit == 0U)
        {
            return false;
        }

        while (!state.AttemptEpochMs.empty())
        {
            const std::uint64_t attemptEpoch{state.AttemptEpochMs.front()};
            if (registration.RestartWindowMs > 0U &&
                nowEpochMs > attemptEpoch &&
                (nowEpochMs - attemptEpoch) > registration.RestartWindowMs)
            {
                state.AttemptEpochMs.pop_front();
            }
            else
            {
                break;
            }
        }

        return state.AttemptEpochMs.size() < registration.RestartLimit;
    }

    bool TriggerRestartRecovery(
        const AppRegistration &registration,
        pid_t &newPid)
    {
        if (registration.RestartCommand.empty())
        {
            return false;
        }

        const std::string instancePath{
            registration.InstanceSpecifier.empty()
                ? registration.Name
                : registration.InstanceSpecifier};

        auto instanceResult{ara::core::InstanceSpecifier::Create(instancePath)};
        if (!instanceResult.HasValue())
        {
            return false;
        }

        bool restarted{false};
        ara::phm::RestartRecoveryAction recoveryAction{
            instanceResult.Value(),
            [&](const ara::core::InstanceSpecifier &)
            {
                restarted = SpawnCommand(registration.RestartCommand, newPid);
            }};
        (void)recoveryAction.Offer();

        ara::exec::ExecutionErrorEvent errorEvent{};
        errorEvent.executionError = 1U;
        errorEvent.functionGroup = nullptr;

        recoveryAction.RecoveryHandler(
            errorEvent,
            ara::phm::TypeOfSupervision::AliveSupervision);
        recoveryAction.StopOffer();
        return restarted;
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string registryFile{
        GetEnvOrDefault(
            "AUTOSAR_USER_APP_REGISTRY_FILE",
            "/run/autosar/user_apps_registry.csv")};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_USER_APP_MONITOR_STATUS_FILE",
            "/run/autosar/user_app_monitor.status")};
    const std::string phmHealthRoot{
        GetEnvOrDefault(
            "AUTOSAR_PHM_HEALTH_DIR",
            "/run/autosar/phm/health")};
    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_USER_APP_MONITOR_PERIOD_MS", 1000U, 600000U)};
    const std::uint32_t heartbeatGraceMs{
        GetEnvU32("AUTOSAR_USER_APP_HEARTBEAT_GRACE_MS", 500U, 600000U)};
    const std::uint32_t startupGraceMs{
        GetEnvU32("AUTOSAR_USER_APP_MONITOR_STARTUP_GRACE_MS", 3000U, 600000U)};
    const std::uint32_t restartBackoffMs{
        GetEnvU32("AUTOSAR_USER_APP_MONITOR_RESTART_BACKOFF_MS", 1000U, 600000U)};
    const bool enforceHealth{
        GetEnvBool("AUTOSAR_USER_APP_MONITOR_ENFORCE_HEALTH", true)};
    const bool restartOnFailure{
        GetEnvBool("AUTOSAR_USER_APP_MONITOR_RESTART_ON_FAILURE", true)};
    const bool allowDeactivatedAsHealthy{
        GetEnvBool(
            "AUTOSAR_USER_APP_MONITOR_ALLOW_DEACTIVATED_AS_HEALTHY",
            true)};
    const int killSignal{ResolveKillSignal()};

    EnsureDirectoryForFile(registryFile);
    EnsureDirectoryForFile(statusFile);
    EnsureDirectoryTree(phmHealthRoot);
    ::mkdir("/run/autosar", 0755);

    while (gRunning.load())
    {
        MonitorSummary summary;
        std::vector<AppStatus> appStatuses;

        summary.InvalidRows = 0U;
        auto registrations{LoadRegistry(registryFile, summary.InvalidRows)};
        summary.RegisteredApps = registrations.size();
        appStatuses.reserve(registrations.size());

        bool registryUpdated{false};

        for (auto &registration : registrations)
        {
            AppStatus status;
            status.Registration = registration;
            status.Alive = IsProcessAlive(registration.Pid, &status.ZombieDetected);

            const std::string runtimeKey{BuildRuntimeStateKey(registration)};
            auto &runtimeState{gRestartState[runtimeKey]};
            const std::uint64_t nowEpochMs{NowEpochMs()};

            if (registration.Pid > 1 &&
                registration.Pid != runtimeState.LastSeenPid)
            {
                runtimeState.LastSeenPid = registration.Pid;
                runtimeState.LastPidChangeEpochMs = nowEpochMs;
            }

            const bool startupGraceApplied{
                status.Alive &&
                IsWithinStartupGrace(
                    runtimeState,
                    nowEpochMs,
                    startupGraceMs)};
            status.StartupGraceApplied = startupGraceApplied;
            if (startupGraceApplied)
            {
                ++summary.StartupGraceApps;
            }

            status.HeartbeatFresh = IsHeartbeatFresh(
                registration,
                heartbeatGraceMs,
                status.HeartbeatChecked);
            if (startupGraceApplied &&
                status.HeartbeatChecked &&
                !status.HeartbeatFresh)
            {
                status.HeartbeatFresh = true;
            }

            PhmStatusSample phmStatus;
            status.PhmChecked = TryReadPhmStatus(phmHealthRoot, registration, phmStatus);
            if (status.PhmChecked)
            {
                status.PhmStatusCode = phmStatus.StatusCode;
                status.PhmStatusHealthy = IsPhmStatusHealthy(
                    phmStatus,
                    allowDeactivatedAsHealthy);
                status.PhmFresh = IsPhmStatusFresh(registration, phmStatus, heartbeatGraceMs);
                if (startupGraceApplied && !status.PhmFresh)
                {
                    status.PhmFresh = true;
                }
            }

            if (status.Alive)
            {
                ++summary.AliveApps;
            }
            if (status.ZombieDetected)
            {
                ++summary.ZombieApps;
            }

            if (status.HeartbeatChecked)
            {
                ++summary.HeartbeatChecks;
                if (!status.HeartbeatFresh)
                {
                    ++summary.HeartbeatFailures;
                }
            }

            if (status.PhmChecked)
            {
                ++summary.PhmChecks;
                if (status.PhmStatusCode == 3U)
                {
                    ++summary.PhmDeactivatedApps;
                }
                if (!status.PhmStatusHealthy || !status.PhmFresh)
                {
                    ++summary.PhmFailures;
                }
            }

            const bool deactivatedStopAllowed{
                allowDeactivatedAsHealthy &&
                !status.Alive &&
                status.PhmChecked &&
                status.PhmStatusCode == 3U};
            status.DeactivatedStopAllowed = deactivatedStopAllowed;

            const bool healthy{
                deactivatedStopAllowed ||
                (status.Alive &&
                 (!status.HeartbeatChecked || status.HeartbeatFresh) &&
                 (!status.PhmChecked || (status.PhmStatusHealthy && status.PhmFresh)))};

            if (healthy)
            {
                ++summary.HealthyApps;
            }
            else
            {
                ++summary.UnhealthyApps;
            }

            const bool needsRecovery{!healthy};
            if (enforceHealth && needsRecovery)
            {
                status.RecoveryTriggered = true;

                if (status.Alive)
                {
                    TryTerminateProcess(registration.Pid, killSignal, summary.KilledApps);
                    status.Alive = false;
                }

                if (restartOnFailure && !registration.RestartCommand.empty())
                {
                    const bool restartAllowed{
                        CanAttemptRestart(
                            registration,
                            nowEpochMs,
                            runtimeState)};

                    bool restartAllowedWithBackoff{restartAllowed};
                    if (restartAllowed &&
                        restartBackoffMs > 0U &&
                        !runtimeState.AttemptEpochMs.empty())
                    {
                        const std::uint64_t lastAttemptEpoch{
                            runtimeState.AttemptEpochMs.back()};
                        if (nowEpochMs >= lastAttemptEpoch &&
                            (nowEpochMs - lastAttemptEpoch) <
                                static_cast<std::uint64_t>(restartBackoffMs))
                        {
                            restartAllowedWithBackoff = false;
                            status.RestartBackoffActive = true;
                            ++summary.RestartBackoffSuppressions;
                        }
                    }

                    if (restartAllowedWithBackoff)
                    {
                        ++summary.RestartAttempts;
                        runtimeState.AttemptEpochMs.push_back(nowEpochMs);

                        pid_t newPid{0};
                        if (TriggerRestartRecovery(registration, newPid) && newPid > 1)
                        {
                            registration.Pid = newPid;
                            status.Registration.Pid = newPid;
                            status.Restarted = true;
                            ++summary.RestartSuccesses;
                            runtimeState.LastSeenPid = newPid;
                            runtimeState.LastPidChangeEpochMs = NowEpochMs();
                            registryUpdated = true;
                        }
                    }
                    else
                    {
                        status.RestartSuppressed = true;
                        ++summary.RestartSuppressed;
                    }
                }
            }

            appStatuses.emplace_back(std::move(status));
        }

        if (registryUpdated)
        {
            (void)WriteRegistry(registryFile, registrations);
        }

        PruneRuntimeState(registrations);
        WriteStatus(statusFile, summary, appStatuses);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    return 0;
}
