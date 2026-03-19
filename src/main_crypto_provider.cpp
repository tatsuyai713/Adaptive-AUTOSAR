/// @file src/main_crypto_provider.cpp
/// @brief Resident daemon that provides AUTOSAR Cryptography key management
///        and cryptographic operations as a platform service.
/// @details Initializes the HSM (or software-emulated) crypto provider,
///          manages key slots per configured policy, supports key rotation,
///          and writes periodic status to /run/autosar/crypto_provider.status.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

#include "./ara/core/initialization.h"
#include "./ara/crypto/hsm_provider.h"
#include "./ara/crypto/key_storage_provider.h"

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
            if (parsed > 600000U)
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

    void EnsureRunDirectory()
    {
        ::mkdir("/run/autosar", 0755);
    }

    const char *SlotStatusString(ara::crypto::HsmSlotStatus status)
    {
        switch (status)
        {
        case ara::crypto::HsmSlotStatus::kEmpty:
            return "Empty";
        case ara::crypto::HsmSlotStatus::kOccupied:
            return "Occupied";
        case ara::crypto::HsmSlotStatus::kLocked:
            return "Locked";
        default:
            return "Unknown";
        }
    }

    void WriteStatus(
        const std::string &statusFile,
        const ara::crypto::HsmProvider &hsm,
        std::size_t selfTestsPassed,
        std::size_t selfTestsFailed,
        std::size_t keyRotations)
    {
        std::ofstream stream(statusFile);
        if (!stream.is_open())
        {
            return;
        }

        stream << "initialized=" << (hsm.IsInitialized() ? "true" : "false")
               << "\n";

        const auto slots{hsm.GetAllSlots()};
        stream << "total_slots=" << slots.size() << "\n";

        std::size_t occupied{0U};
        for (const auto &slot : slots)
        {
            if (slot.Status == ara::crypto::HsmSlotStatus::kOccupied)
            {
                ++occupied;
            }
        }
        stream << "occupied_slots=" << occupied << "\n";
        stream << "self_tests_passed=" << selfTestsPassed << "\n";
        stream << "self_tests_failed=" << selfTestsFailed << "\n";
        stream << "key_rotations=" << keyRotations << "\n";
        stream << "updated_epoch_ms=" << NowEpochMs() << "\n";
    }

    /// Run a self-test: generate a random key, encrypt, decrypt, verify.
    bool RunCryptoSelfTest(ara::crypto::HsmProvider &hsm)
    {
        auto keyResult{hsm.GenerateKey(
            ara::crypto::HsmAlgorithm::kAes128, "_selftest_temp")};
        if (!keyResult.HasValue())
        {
            return false;
        }

        const std::uint32_t testSlot{keyResult.Value()};
        const std::vector<std::uint8_t> testPlain{
            0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

        bool passed{false};
        auto encResult{hsm.Encrypt(testSlot, testPlain)};
        if (encResult.Success)
        {
            auto decResult{hsm.Decrypt(testSlot, encResult.OutputData)};
            passed = decResult.Success && (decResult.OutputData == testPlain);
        }

        (void)hsm.DeleteKey(testSlot);
        return passed;
    }
}

int main()
{
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);

    const std::string tokenLabel{
        GetEnvOrDefault("AUTOSAR_CRYPTO_TOKEN", "AutosarEcuHsm")};
    const std::uint32_t periodMs{
        GetEnvU32("AUTOSAR_CRYPTO_PERIOD_MS", 5000U)};
    const bool enableKeyRotation{
        GetEnvBool("AUTOSAR_CRYPTO_KEY_ROTATION", false)};
    const std::uint32_t keyRotationIntervalMs{
        GetEnvU32("AUTOSAR_CRYPTO_ROTATION_INTERVAL_MS", 3600000U)};
    const bool enableSelfTest{
        GetEnvBool("AUTOSAR_CRYPTO_SELF_TEST", true)};
    const std::uint32_t selfTestIntervalMs{
        GetEnvU32("AUTOSAR_CRYPTO_SELF_TEST_INTERVAL_MS", 60000U)};
    const std::string statusFile{
        GetEnvOrDefault(
            "AUTOSAR_CRYPTO_STATUS_FILE",
            "/run/autosar/crypto_provider.status")};
    const std::string keyStoragePath{
        GetEnvOrDefault(
            "AUTOSAR_CRYPTO_KEY_STORAGE_PATH",
            "/var/lib/autosar/crypto/keys")};

    EnsureRunDirectory();

    auto initResult{ara::core::Initialize()};
    if (!initResult.HasValue())
    {
        return 1;
    }

    // Initialize HSM provider.
    ara::crypto::HsmProvider hsm;
    auto hsmInitResult{hsm.Initialize(tokenLabel)};
    if (!hsmInitResult.HasValue())
    {
        (void)ara::core::Deinitialize();
        return 1;
    }

    // Pre-generate platform keys for common operations.
    std::uint32_t platformAesSlot{0U};
    std::uint32_t platformHmacSlot{0U};
    {
        auto aesResult{hsm.GenerateKey(
            ara::crypto::HsmAlgorithm::kAes128, "platform_aes")};
        if (aesResult.HasValue())
        {
            platformAesSlot = aesResult.Value();
        }

        auto hmacResult{hsm.GenerateKey(
            ara::crypto::HsmAlgorithm::kHmacSha256, "platform_hmac")};
        if (hmacResult.HasValue())
        {
            platformHmacSlot = hmacResult.Value();
        }
    }

    std::size_t selfTestsPassed{0U};
    std::size_t selfTestsFailed{0U};
    std::size_t keyRotations{0U};

    // Initial self-test.
    if (enableSelfTest)
    {
        if (RunCryptoSelfTest(hsm))
        {
            ++selfTestsPassed;
        }
        else
        {
            ++selfTestsFailed;
        }
    }

    std::uint64_t lastSelfTestMs{NowEpochMs()};
    std::uint64_t lastKeyRotationMs{NowEpochMs()};

    while (gRunning.load())
    {
        const std::uint64_t nowMs{NowEpochMs()};

        // Periodic self-test.
        if (enableSelfTest &&
            (nowMs - lastSelfTestMs) >= selfTestIntervalMs)
        {
            if (RunCryptoSelfTest(hsm))
            {
                ++selfTestsPassed;
            }
            else
            {
                ++selfTestsFailed;
            }
            lastSelfTestMs = nowMs;
        }

        // Key rotation: regenerate platform keys.
        if (enableKeyRotation &&
            (nowMs - lastKeyRotationMs) >= keyRotationIntervalMs)
        {
            if (platformAesSlot > 0U)
            {
                (void)hsm.DeleteKey(platformAesSlot);
                auto aesResult{hsm.GenerateKey(
                    ara::crypto::HsmAlgorithm::kAes128, "platform_aes")};
                if (aesResult.HasValue())
                {
                    platformAesSlot = aesResult.Value();
                    ++keyRotations;
                }
            }

            if (platformHmacSlot > 0U)
            {
                (void)hsm.DeleteKey(platformHmacSlot);
                auto hmacResult{hsm.GenerateKey(
                    ara::crypto::HsmAlgorithm::kHmacSha256, "platform_hmac")};
                if (hmacResult.HasValue())
                {
                    platformHmacSlot = hmacResult.Value();
                    ++keyRotations;
                }
            }

            lastKeyRotationMs = nowMs;
        }

        WriteStatus(statusFile, hsm, selfTestsPassed,
                    selfTestsFailed, keyRotations);

        const std::uint32_t sleepStepMs{100U};
        std::uint32_t sleptMs{0U};
        while (gRunning.load() && sleptMs < periodMs)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleepStepMs));
            sleptMs += sleepStepMs;
        }
    }

    hsm.Finalize();
    WriteStatus(statusFile, hsm, selfTestsPassed,
                selfTestsFailed, keyRotations);

    (void)ara::core::Deinitialize();
    return 0;
}
