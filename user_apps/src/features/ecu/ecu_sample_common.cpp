#include <algorithm>
#include <chrono>
#include <limits>

#include "./ecu_sample_common.h"
#include "ara/core/instance_specifier.h"

namespace
{
    // Accept several user-friendly spellings for booleans.
    bool IsTrueText(const std::string &value)
    {
        return value == "1" ||
               value == "true" ||
               value == "TRUE" ||
               value == "on" ||
               value == "ON";
    }

    bool IsFalseText(const std::string &value)
    {
        return value == "0" ||
               value == "false" ||
               value == "FALSE" ||
               value == "off" ||
               value == "OFF";
    }
}

namespace sample
{
    namespace ecu_showcase
    {
        bool TryReadArgument(
            int argc,
            char *argv[],
            const std::string &name,
            std::string &value)
        {
            // Expected command-line shape: --key=value
            const std::string prefix{name + "="};
            for (int index{1}; index < argc; ++index)
            {
                const std::string argument{argv[index]};
                if (argument.find(prefix) == 0U)
                {
                    value = argument.substr(prefix.size());
                    return true;
                }
            }

            return false;
        }

        std::uint32_t ParseU32(
            const std::string &text,
            std::uint32_t fallback) noexcept
        {
            if (text.empty())
            {
                return fallback;
            }

            try
            {
                // Base 0 lets users pass decimal (100) or hex (0x64).
                std::size_t parsedLength{0U};
                const auto parsed{std::stoul(text, &parsedLength, 0)};
                if (parsedLength != text.size())
                {
                    return fallback;
                }
                if (parsed > static_cast<unsigned long>(std::numeric_limits<std::uint32_t>::max()))
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

        std::uint64_t ParseU64(
            const std::string &text,
            std::uint64_t fallback) noexcept
        {
            if (text.empty())
            {
                return fallback;
            }

            try
            {
                // Base 0 lets users pass decimal (100) or hex (0x64).
                std::size_t parsedLength{0U};
                const auto parsed{std::stoull(text, &parsedLength, 0)};
                if (parsedLength != text.size())
                {
                    return fallback;
                }

                return static_cast<std::uint64_t>(parsed);
            }
            catch (...)
            {
                return fallback;
            }
        }

        bool ParseBool(
            const std::string &text,
            bool fallback) noexcept
        {
            if (text.empty())
            {
                return fallback;
            }

            if (IsTrueText(text))
            {
                return true;
            }
            if (IsFalseText(text))
            {
                return false;
            }

            return fallback;
        }

        HealthReporter::HealthReporter(const std::string &instanceSpecifier) noexcept
        {
            // If specifier creation fails, keep this helper disabled gracefully.
            auto specifierResult{ara::core::InstanceSpecifier::Create(instanceSpecifier)};
            if (!specifierResult.HasValue())
            {
                return;
            }

            mChannel.reset(
                new ara::phm::HealthChannel{specifierResult.Value()});
        }

        void HealthReporter::ReportOk() noexcept
        {
            if (mChannel)
            {
                // Report normal health status to PHM.
                (void)mChannel->ReportHealthStatus(
                    ara::phm::HealthStatus::kOk);
            }
        }

        void HealthReporter::ReportFailed() noexcept
        {
            if (mChannel)
            {
                // Report failure state to PHM.
                (void)mChannel->ReportHealthStatus(
                    ara::phm::HealthStatus::kFailed);
            }
        }

        void HealthReporter::ReportDeactivated() noexcept
        {
            if (mChannel)
            {
                // Report graceful shutdown/deactivated state.
                (void)mChannel->ReportHealthStatus(
                    ara::phm::HealthStatus::kDeactivated);
            }
        }

        PersistentCounterStore::PersistentCounterStore(
            const std::string &instanceSpecifier) noexcept
        {
            // If storage setup fails, methods stay safe no-op.
            auto specifierResult{ara::core::InstanceSpecifier::Create(instanceSpecifier)};
            if (!specifierResult.HasValue())
            {
                return;
            }

            auto storageResult{ara::per::OpenKeyValueStorage(specifierResult.Value())};
            if (!storageResult.HasValue())
            {
                return;
            }

            mStorage = storageResult.Value();
        }

        bool PersistentCounterStore::IsAvailable() const noexcept
        {
            return static_cast<bool>(mStorage);
        }

        std::uint64_t PersistentCounterStore::Load(
            const std::string &key,
            std::uint64_t fallbackValue) const noexcept
        {
            if (!mStorage)
            {
                return fallbackValue;
            }

            auto valueResult{mStorage->GetValue<std::uint64_t>(key)};
            if (!valueResult.HasValue())
            {
                // Return default if key does not exist yet.
                return fallbackValue;
            }

            return valueResult.Value();
        }

        void PersistentCounterStore::Save(
            const std::string &key,
            std::uint64_t value) noexcept
        {
            if (!mStorage)
            {
                return;
            }

            // Buffer value in memory; call Sync() to flush to file.
            (void)mStorage->SetValue<std::uint64_t>(key, value);
        }

        void PersistentCounterStore::Sync() noexcept
        {
            if (mStorage)
            {
                // Persist buffered values to backing storage.
                (void)mStorage->SyncToStorage();
            }
        }

        sample::ara_com_pubsub::portable::BackendProfile BuildDdsProfile(
            std::uint32_t domainId,
            const std::string &topicName)
        {
            // Configure portable API to publish through the DDS transport.
            sample::ara_com_pubsub::portable::BackendProfile profile;
            profile.EventBinding =
                sample::ara_com_pubsub::portable::EventBackend::kDds;
            profile.ZeroCopyBinding =
                sample::ara_com_pubsub::portable::ZeroCopyBackend::kNone;
            profile.DdsDomainId = domainId;
            profile.DdsTopicName = topicName;
            return profile;
        }

        sample::ara_com_pubsub::portable::BackendProfile BuildSomeIpProfile()
        {
            // Configure portable API to consume/offer through SOME/IP.
            sample::ara_com_pubsub::portable::BackendProfile profile;
            profile.EventBinding =
                sample::ara_com_pubsub::portable::EventBackend::kSomeIp;
            profile.ZeroCopyBinding =
                sample::ara_com_pubsub::portable::ZeroCopyBackend::kNone;
            return profile;
        }

        sample::ara_com_pubsub::VehicleStatusFrame ToPortableFrame(
            const sample::vehicle_status::VehicleStatusFrame &frame) noexcept
        {
            // Explicit field-by-field conversion keeps layout assumptions clear.
            sample::ara_com_pubsub::VehicleStatusFrame converted{};
            converted.SequenceCounter = frame.SequenceCounter;
            converted.SpeedCentiKph = frame.SpeedCentiKph;
            converted.EngineRpm = frame.EngineRpm;
            converted.SteeringAngleCentiDeg = frame.SteeringAngleCentiDeg;
            converted.Gear = frame.Gear;
            converted.StatusFlags = frame.StatusFlags;
            return converted;
        }
    }
}
