/// @file src/main_persistency_guard.cpp
/// @brief Resident daemon that periodically syncs configured persistency stores.

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

#include "./ara/core/instance_specifier.h"
#include "./ara/per/persistency.h"

namespace
{
    struct StorageContext
    {
        std::string Specifier;
        ara::per::SharedHandle<ara::per::KeyValueStorage> Handle;
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

    std::vector<std::string> SplitSpecifiers(const std::string &raw)
    {
        std::vector<std::string> result;
        std::stringstream stream(raw);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            token.erase(
                token.begin(),
                std::find_if(
                    token.begin(),
                    token.end(),
                    [](int ch)
                    { return !std::isspace(ch); }));
            token.erase(
                std::find_if(
                    token.rbegin(),
                    token.rend(),
                    [](int ch)
                    { return !std::isspace(ch); })
                    .base(),
                token.end());
            if (!token.empty())
            {
                result.emplace_back(token);
            }
        }
        return result;
    }

    bool TryAttachStorage(
        const std::string &specifierText,
        StorageContext &context)
    {
        const auto specifierResult{
            ara::core::InstanceSpecifier::Create(specifierText)};
        if (!specifierResult.HasValue())
        {
            return false;
        }

        auto storageResult{
            ara::per::OpenKeyValueStorage(specifierResult.Value())};
        if (!storageResult.HasValue())
        {
            return false;
        }

        context.Specifier = specifierText;
        context.Handle = storageResult.Value();
        return true;
    }

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    void WriteStatus(
        const std::string &statusFile,
        std::size_t attachedCount,
        std::size_t syncedCount)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "attached_storages=" << attachedCount << "\n";
        stream << "synced_storages=" << syncedCount << "\n";
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

    const std::uint32_t syncPeriodMs{
        GetEnvU32("AUTOSAR_PERSISTENCY_SYNC_PERIOD_MS", 5000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_PERSISTENCY_STATUS_FILE",
            "/run/autosar/persistency_guard.status")};
    const std::string rawSpecifiers{
        GetEnvOrDefault(
            "AUTOSAR_PERSISTENCY_SPECIFIERS",
            "PlatformState,DiagnosticState,ExecutionState")};

    EnsureRunDirectory();

    const std::vector<std::string> specifiers{
        SplitSpecifiers(rawSpecifiers)};
    std::vector<StorageContext> storages;
    storages.reserve(specifiers.size());

    for (const auto &specifierText : specifiers)
    {
        StorageContext context;
        if (TryAttachStorage(specifierText, context))
        {
            storages.emplace_back(std::move(context));
        }
    }

    const std::string heartbeatKey{
        "__persistency_guard_last_sync_epoch_ms"};

    while (gRunning.load())
    {
        std::size_t syncedCount{0U};
        const std::int64_t nowMs{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()};

        for (auto &storage : storages)
        {
            if (!storage.Handle)
            {
                continue;
            }

            auto setResult{
                storage.Handle->SetValue<std::int64_t>(heartbeatKey, nowMs)};
            if (!setResult.HasValue())
            {
                continue;
            }

            auto syncResult{storage.Handle->SyncToStorage()};
            if (syncResult.HasValue())
            {
                ++syncedCount;
            }
        }

        WriteStatus(statusFile, storages.size(), syncedCount);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < syncPeriodMs)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    for (auto &storage : storages)
    {
        if (storage.Handle)
        {
            storage.Handle->SyncToStorage();
        }
    }

    return 0;
}
