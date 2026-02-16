/// @file src/main_ucm_daemon.cpp
/// @brief Resident daemon that runs the AUTOSAR Update & Configuration
///        Management (UCM) service. Watches a staging directory for
///        incoming software packages and processes them through the
///        UCM state machine (stage → verify → activate / rollback).

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "./ara/ucm/update_manager.h"
#include "./ara/crypto/crypto_provider.h"

namespace
{
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
            if (parsed == 0U || parsed > 600000U)
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

    std::uint64_t NowEpochMs()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    void EnsureDirTree(const std::string &path)
    {
        if (path.empty())
        {
            return;
        }

        std::string partial;
        if (path.front() == '/')
        {
            partial = "/";
        }

        std::istringstream stream{path};
        std::string segment;
        while (std::getline(stream, segment, '/'))
        {
            if (segment.empty())
            {
                continue;
            }

            if (!partial.empty() && partial.back() != '/')
            {
                partial.push_back('/');
            }
            partial += segment;
            ::mkdir(partial.c_str(), 0755);
        }
    }

    const char *SessionStateToString(
        ara::ucm::UpdateSessionState state)
    {
        switch (state)
        {
        case ara::ucm::UpdateSessionState::kIdle:
            return "Idle";
        case ara::ucm::UpdateSessionState::kPrepared:
            return "Prepared";
        case ara::ucm::UpdateSessionState::kPackageStaged:
            return "PackageStaged";
        case ara::ucm::UpdateSessionState::kPackageVerified:
            return "PackageVerified";
        case ara::ucm::UpdateSessionState::kActivating:
            return "Activating";
        case ara::ucm::UpdateSessionState::kActivated:
            return "Activated";
        case ara::ucm::UpdateSessionState::kVerificationFailed:
            return "VerificationFailed";
        case ara::ucm::UpdateSessionState::kRolledBack:
            return "RolledBack";
        case ara::ucm::UpdateSessionState::kCancelled:
            return "Cancelled";
        case ara::ucm::UpdateSessionState::kTransferring:
            return "Transferring";
        default:
            return "Unknown";
        }
    }

    /// Simple manifest format for a staged package:
    ///   package_name=<name>
    ///   target_cluster=<cluster>
    ///   version=<version>
    ///   payload_file=<relative path>
    ///   digest_sha256=<hex string>
    struct PackageManifest
    {
        std::string PackageName;
        std::string TargetCluster;
        std::string Version;
        std::string PayloadFile;
        std::string DigestSha256Hex;
    };

    bool ParseManifest(const std::string &manifestPath,
                       PackageManifest &manifest)
    {
        std::ifstream stream(manifestPath);
        if (!stream.is_open())
        {
            return false;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            const auto delim{line.find('=')};
            if (delim == std::string::npos)
            {
                continue;
            }

            const std::string key{line.substr(0U, delim)};
            const std::string value{line.substr(delim + 1U)};

            if (key == "package_name")
            {
                manifest.PackageName = value;
            }
            else if (key == "target_cluster")
            {
                manifest.TargetCluster = value;
            }
            else if (key == "version")
            {
                manifest.Version = value;
            }
            else if (key == "payload_file")
            {
                manifest.PayloadFile = value;
            }
            else if (key == "digest_sha256")
            {
                manifest.DigestSha256Hex = value;
            }
        }

        return !manifest.PackageName.empty() &&
               !manifest.TargetCluster.empty() &&
               !manifest.Version.empty() &&
               !manifest.PayloadFile.empty() &&
               manifest.DigestSha256Hex.size() == 64U;
    }

    std::vector<std::uint8_t> HexToBytes(const std::string &hex)
    {
        std::vector<std::uint8_t> bytes;
        if (hex.size() % 2U != 0U)
        {
            return bytes;
        }

        bytes.reserve(hex.size() / 2U);
        for (std::size_t i = 0U; i < hex.size(); i += 2U)
        {
            const std::string byteStr{hex.substr(i, 2U)};
            try
            {
                bytes.push_back(static_cast<std::uint8_t>(
                    std::stoul(byteStr, nullptr, 16)));
            }
            catch (...)
            {
                return {};
            }
        }
        return bytes;
    }

    std::vector<std::uint8_t> ReadBinaryFile(const std::string &path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream.is_open())
        {
            return {};
        }

        const auto size{stream.tellg()};
        if (size <= 0)
        {
            return {};
        }

        stream.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char *>(data.data()),
                    static_cast<std::streamsize>(size));
        return data;
    }

    /// List *.manifest files in staging directory.
    std::vector<std::string> ListManifestFiles(
        const std::string &stagingDir)
    {
        std::vector<std::string> manifests;
        DIR *dir{::opendir(stagingDir.c_str())};
        if (dir == nullptr)
        {
            return manifests;
        }

        struct dirent *entry{nullptr};
        while ((entry = ::readdir(dir)) != nullptr)
        {
            const std::string name{entry->d_name};
            const std::string suffix{".manifest"};
            if (name.size() > suffix.size() &&
                name.compare(name.size() - suffix.size(),
                             suffix.size(), suffix) == 0)
            {
                manifests.push_back(stagingDir + "/" + name);
            }
        }

        ::closedir(dir);
        return manifests;
    }

    void MoveToProcessed(const std::string &manifestPath,
                         const std::string &processedDir,
                         bool success)
    {
        const auto nameStart{manifestPath.rfind('/')};
        const std::string name{
            nameStart == std::string::npos
                ? manifestPath
                : manifestPath.substr(nameStart + 1U)};
        const std::string suffix{success ? ".done" : ".failed"};
        const std::string dest{processedDir + "/" + name + suffix};
        ::rename(manifestPath.c_str(), dest.c_str());
    }

    void WriteStatus(
        const std::string &statusFile,
        const ara::ucm::UpdateManager &manager,
        std::size_t processedCount,
        std::size_t failedCount,
        const std::string &lastError)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "session_state="
               << SessionStateToString(manager.GetState()) << "\n";
        stream << "session_id=" << manager.GetSessionId() << "\n";
        stream << "progress=" << static_cast<unsigned>(manager.GetProgress())
               << "\n";
        stream << "active_version=" << manager.GetActiveVersion() << "\n";

        const auto clusters{manager.GetKnownClusters()};
        stream << "known_clusters=" << clusters.size() << "\n";
        for (std::size_t i = 0U; i < clusters.size(); ++i)
        {
            const auto verResult{manager.GetClusterVersion(clusters[i])};
            stream << "cluster[" << i << "].name=" << clusters[i] << "\n";
            stream << "cluster[" << i << "].version="
                   << (verResult.HasValue() ? verResult.Value() : "unknown")
                   << "\n";
        }

        stream << "processed_count=" << processedCount << "\n";
        stream << "failed_count=" << failedCount << "\n";
        if (!lastError.empty())
        {
            stream << "last_error=" << lastError << "\n";
        }

        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string stagingDir{
        GetEnvOrDefault(
            "AUTOSAR_UCM_STAGING_DIR",
            "/var/lib/autosar/ucm/staging")};
    const std::string processedDir{
        GetEnvOrDefault(
            "AUTOSAR_UCM_PROCESSED_DIR",
            "/var/lib/autosar/ucm/processed")};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_UCM_STATUS_FILE",
            "/run/autosar/ucm_daemon.status")};
    const std::uint32_t scanPeriodMs{
        GetEnvU32("AUTOSAR_UCM_SCAN_PERIOD_MS", 2000U)};
    const bool autoActivate{
        GetEnvBool("AUTOSAR_UCM_AUTO_ACTIVATE", true)};

    EnsureDirTree(stagingDir);
    EnsureDirTree(processedDir);
    ::mkdir("/run/autosar", 0755);

    ara::ucm::UpdateManager manager;

    std::size_t processedCount{0U};
    std::size_t failedCount{0U};
    std::string lastError;
    std::uint64_t sessionCounter{0U};

    while (gRunning.load())
    {
        // Scan for new manifests in staging directory.
        const auto manifests{ListManifestFiles(stagingDir)};

        for (const auto &manifestPath : manifests)
        {
            if (!gRunning.load())
            {
                break;
            }

            PackageManifest pkgManifest;
            if (!ParseManifest(manifestPath, pkgManifest))
            {
                lastError = "invalid manifest: " + manifestPath;
                ++failedCount;
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            // Resolve payload file path relative to staging directory.
            std::string payloadPath{pkgManifest.PayloadFile};
            if (!payloadPath.empty() && payloadPath.front() != '/')
            {
                payloadPath = stagingDir + "/" + payloadPath;
            }

            const auto payload{ReadBinaryFile(payloadPath)};
            if (payload.empty())
            {
                lastError = "payload read error: " + payloadPath;
                ++failedCount;
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            const auto digest{HexToBytes(pkgManifest.DigestSha256Hex)};
            if (digest.size() != 32U)
            {
                lastError = "invalid digest hex";
                ++failedCount;
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            // Generate session ID.
            ++sessionCounter;
            const std::string sessionId{
                "ucm-session-" + std::to_string(NowEpochMs()) +
                "-" + std::to_string(sessionCounter)};

            // If a previous session is still active, cancel it first.
            if (manager.GetState() != ara::ucm::UpdateSessionState::kIdle &&
                manager.GetState() != ara::ucm::UpdateSessionState::kActivated &&
                manager.GetState() != ara::ucm::UpdateSessionState::kRolledBack &&
                manager.GetState() != ara::ucm::UpdateSessionState::kCancelled)
            {
                (void)manager.CancelUpdateSession();
            }

            // Prepare → Stage → Verify → Activate pipeline.
            auto prepareResult{manager.PrepareUpdate(sessionId)};
            if (!prepareResult.HasValue())
            {
                lastError = "prepare failed";
                ++failedCount;
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            ara::ucm::SoftwarePackageMetadata metadata;
            metadata.PackageName = pkgManifest.PackageName;
            metadata.TargetCluster = pkgManifest.TargetCluster;
            metadata.Version = pkgManifest.Version;

            auto stageResult{
                manager.StageSoftwarePackage(metadata, payload, digest)};
            if (!stageResult.HasValue())
            {
                lastError = "stage failed for " + pkgManifest.PackageName;
                ++failedCount;
                (void)manager.CancelUpdateSession();
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            auto verifyResult{manager.VerifyStagedSoftwarePackage()};
            if (!verifyResult.HasValue())
            {
                lastError = "verify failed for " + pkgManifest.PackageName;
                ++failedCount;
                (void)manager.CancelUpdateSession();
                MoveToProcessed(manifestPath, processedDir, false);
                continue;
            }

            if (autoActivate)
            {
                auto activateResult{manager.ActivateSoftwarePackage()};
                if (!activateResult.HasValue())
                {
                    lastError = "activate failed for " +
                                pkgManifest.PackageName;
                    ++failedCount;
                    (void)manager.RollbackSoftwarePackage();
                    MoveToProcessed(manifestPath, processedDir, false);
                    continue;
                }
            }

            ++processedCount;
            lastError.clear();

            // Clean up payload file after successful processing.
            ::remove(payloadPath.c_str());
            MoveToProcessed(manifestPath, processedDir, true);
        }

        WriteStatus(statusFile, manager, processedCount, failedCount,
                    lastError);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < scanPeriodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    WriteStatus(statusFile, manager, processedCount, failedCount, lastError);
    return 0;
}
