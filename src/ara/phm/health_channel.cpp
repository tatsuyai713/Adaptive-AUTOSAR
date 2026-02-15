/// @file src/ara/phm/health_channel.cpp
/// @brief Implementation for health channel.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./health_channel.h"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace ara
{
    namespace phm
    {
        namespace
        {
            std::string GetHealthRuntimeRoot()
            {
                const char *configuredRoot{std::getenv("AUTOSAR_PHM_HEALTH_DIR")};
                if (configuredRoot != nullptr && configuredRoot[0] != '\0')
                {
                    return std::string{configuredRoot};
                }

                return "/run/autosar/phm/health";
            }

            std::string SanitizeInstancePath(const std::string &instance)
            {
                std::string sanitized;
                sanitized.reserve(instance.size());

                for (const char ch : instance)
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
                    sanitized = "unknown_instance";
                }

                return sanitized;
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
                    const std::size_t delimiter{
                        normalized.find('/', begin)};
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

            std::uint64_t CurrentEpochMs()
            {
                return static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
            }

            std::string BuildHealthFilePath(const core::InstanceSpecifier &instance)
            {
                const std::string root{GetHealthRuntimeRoot()};
                const std::string filename{
                    SanitizeInstancePath(instance.ToString()) + ".status"};
                if (!root.empty() && root.back() == '/')
                {
                    return root + filename;
                }
                return root + "/" + filename;
            }
        }

        HealthChannel::HealthChannel(
            core::InstanceSpecifier instance) : mInstance{std::move(instance)},
                                                mLastReportedStatus{HealthStatus::kOk},
                                                mOffered{false}
        {
        }

        HealthChannel::HealthChannel(HealthChannel &&other) noexcept
            : mInstance{std::move(other.mInstance)},
              mLastReportedStatus{other.mLastReportedStatus},
              mOffered{other.mOffered}
        {
            other.mOffered = false;
        }

        core::Result<void> HealthChannel::Offer()
        {
            if (mOffered)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kAlreadyOffered));
            }
            mOffered = true;
            return core::Result<void>::FromValue();
        }

        void HealthChannel::StopOffer()
        {
            mOffered = false;
        }

        bool HealthChannel::IsOffered() const noexcept
        {
            return mOffered;
        }

        core::Result<void> HealthChannel::ReportHealthStatus(HealthStatus status)
        {
            if (!mOffered)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kNotOffered));
            }

            mLastReportedStatus = status;

            const std::string statusFilePath{BuildHealthFilePath(mInstance)};
            const auto separator{statusFilePath.rfind('/')};
            if (separator != std::string::npos)
            {
                EnsureDirectoryTree(statusFilePath.substr(0U, separator));
            }

            const std::string tempFilePath{statusFilePath + ".tmp"};
            std::ofstream stream(tempFilePath);
            if (stream.is_open())
            {
                stream << "instance=" << mInstance.ToString() << "\n";
                stream << "status=" << static_cast<std::uint32_t>(status) << "\n";
                stream << "updated_epoch_ms=" << CurrentEpochMs() << "\n";
                stream.close();
                ::rename(tempFilePath.c_str(), statusFilePath.c_str());
            }

            return core::Result<void>::FromValue();
        }

        HealthStatus HealthChannel::GetLastReportedStatus() const noexcept
        {
            return mLastReportedStatus;
        }
    }
}
