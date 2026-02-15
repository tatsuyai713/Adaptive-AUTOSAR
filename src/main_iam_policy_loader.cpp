/// @file src/main_iam_policy_loader.cpp
/// @brief Resident daemon that validates and loads IAM policies from file.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/iam/access_control.h"

namespace
{
    struct PolicyRow
    {
        std::string Subject;
        std::string Resource;
        std::string Action;
        ara::iam::PermissionDecision Decision;
    };

    std::atomic_bool gRunning{true};

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

    std::uint32_t GetEnvU32(const char *key, std::uint32_t fallback)
    {
        const char *value{std::getenv(key)};
        if (value == nullptr)
        {
            return fallback;
        }

        try
        {
            const std::uint64_t parsed{std::stoull(value)};
            if (parsed == 0U || parsed > 3600000U)
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

    bool ParseDecision(
        const std::string &text,
        ara::iam::PermissionDecision &decision)
    {
        std::string lowered{text};
        std::transform(
            lowered.begin(),
            lowered.end(),
            lowered.begin(),
            [](unsigned char ch)
            { return static_cast<char>(std::tolower(ch)); });

        if (lowered == "allow")
        {
            decision = ara::iam::PermissionDecision::kAllow;
            return true;
        }
        if (lowered == "deny")
        {
            decision = ara::iam::PermissionDecision::kDeny;
            return true;
        }
        return false;
    }

    std::vector<PolicyRow> LoadPolicyRows(
        const std::string &policyFile,
        std::size_t &invalidRows)
    {
        std::vector<PolicyRow> rows;
        invalidRows = 0U;

        std::ifstream stream(policyFile);
        if (!stream.is_open())
        {
            return rows;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            Trim(line);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            std::stringstream parser(line);
            std::string subject;
            std::string resource;
            std::string action;
            std::string decisionText;
            if (!std::getline(parser, subject, ',') ||
                !std::getline(parser, resource, ',') ||
                !std::getline(parser, action, ',') ||
                !std::getline(parser, decisionText))
            {
                ++invalidRows;
                continue;
            }

            Trim(subject);
            Trim(resource);
            Trim(action);
            Trim(decisionText);

            ara::iam::PermissionDecision decision{
                ara::iam::PermissionDecision::kDeny};
            if (!ParseDecision(decisionText, decision))
            {
                ++invalidRows;
                continue;
            }

            rows.push_back({subject, resource, action, decision});
        }

        return rows;
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    void WriteStatus(
        const std::string &statusFile,
        std::size_t loadedRows,
        std::size_t invalidRows)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "loaded_policies=" << loadedRows << "\n";
        stream << "invalid_rows=" << invalidRows << "\n";
        const auto nowMs{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()};
        stream << "updated_epoch_ms=" << nowMs << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string policyFile{
        GetEnvOrDefault(
            "AUTOSAR_IAM_POLICY_FILE",
            "/etc/autosar/iam_policy.csv")};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_IAM_STATUS_FILE",
            "/run/autosar/iam_policy.status")};
    const std::uint32_t reloadPeriodMs{
        GetEnvU32("AUTOSAR_IAM_RELOAD_PERIOD_MS", 3000U)};

    EnsureRunDirectory();

    ara::iam::AccessControl accessControl;

    while (gRunning.load())
    {
        std::size_t invalidRows{0U};
        const auto rows{LoadPolicyRows(policyFile, invalidRows)};

        accessControl.ClearPolicies();

        std::size_t loadedRows{0U};
        for (const auto &row : rows)
        {
            auto setResult{
                accessControl.SetPolicy(
                    row.Subject,
                    row.Resource,
                    row.Action,
                    row.Decision)};
            if (setResult.HasValue())
            {
                ++loadedRows;
            }
            else
            {
                ++invalidRows;
            }
        }

        WriteStatus(statusFile, loadedRows, invalidRows);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < reloadPeriodMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    return 0;
}
