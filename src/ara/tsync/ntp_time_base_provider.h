/// @file src/ara/tsync/ntp_time_base_provider.h
/// @brief Declarations for NTP time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef NTP_TIME_BASE_PROVIDER_H
#define NTP_TIME_BASE_PROVIDER_H

#include <chrono>
#include <cstdint>
#include <string>
#include "./synchronized_time_base_provider.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief NTP daemon type.
        enum class NtpDaemon : uint8_t
        {
            kChrony = 0, ///< chrony (chronyc).
            kNtpd = 1,   ///< reference ntpd (ntpq).
            kAuto = 2    ///< Auto-detect available daemon.
        };

        /// @brief NTP time source provider (chrony/ntpd integration).
        ///
        /// Queries the local NTP daemon for clock offset information
        /// and uses it to update a TimeSyncClient.
        class NtpTimeBaseProvider : public SynchronizedTimeBaseProvider
        {
        public:
            /// @brief Constructor.
            /// @param daemon NTP daemon type to use (default: auto-detect).
            explicit NtpTimeBaseProvider(NtpDaemon daemon = NtpDaemon::kAuto);

            core::Result<void> UpdateTimeBase(TimeSyncClient &client) override;
            bool IsSourceAvailable() const override;
            const char *GetProviderName() const noexcept override;

            /// @brief Get the NTP clock offset in nanoseconds.
            /// @returns Offset (NTP - system) in nanoseconds, or error.
            core::Result<std::chrono::nanoseconds> GetNtpOffset() const;

            /// @brief Get the detected/configured daemon type.
            NtpDaemon GetActiveDaemon() const noexcept;

            /// @brief Parse chronyc CSV tracking output to extract offset.
            /// @param output Output from `chronyc -c tracking`.
            /// @returns Offset in nanoseconds, or error on parse failure.
            static core::Result<std::chrono::nanoseconds> ParseChronyOutput(
                const std::string &output);

            /// @brief Parse ntpq rv output to extract offset.
            /// @param output Output from `ntpq -c rv 0`.
            /// @returns Offset in nanoseconds, or error on parse failure.
            static core::Result<std::chrono::nanoseconds> ParseNtpqOutput(
                const std::string &output);

        protected:
            /// @brief Execute an external command and return its stdout.
            /// @note Protected virtual for testability.
            virtual core::Result<std::string> RunCommand(
                const std::string &cmd) const;

        private:
            NtpDaemon mConfiguredDaemon;
            mutable NtpDaemon mDetectedDaemon;

            NtpDaemon DetectDaemon() const;
            core::Result<std::chrono::nanoseconds> QueryChronyOffset() const;
            core::Result<std::chrono::nanoseconds> QueryNtpdOffset() const;
        };
    }
}

#endif
