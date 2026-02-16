/// @file src/ara/tsync/ptp_time_base_provider.h
/// @brief Declarations for PTP/gPTP time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PTP_TIME_BASE_PROVIDER_H
#define PTP_TIME_BASE_PROVIDER_H

#include <chrono>
#include <string>
#include "./synchronized_time_base_provider.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief PTP/gPTP time source provider via Linux PTP hardware clock.
        ///
        /// Reads the PTP Hardware Clock (PHC) exposed by ptp4l at
        /// `/dev/ptpN` and computes the offset between PHC and system
        /// clock using `clock_gettime()`.
        class PtpTimeBaseProvider : public SynchronizedTimeBaseProvider
        {
        public:
            /// @brief Constructor.
            /// @param ptpDevice Path to the PTP device (e.g. "/dev/ptp0").
            explicit PtpTimeBaseProvider(
                const std::string &ptpDevice = "/dev/ptp0");

            ~PtpTimeBaseProvider() noexcept override;

            PtpTimeBaseProvider(const PtpTimeBaseProvider &) = delete;
            PtpTimeBaseProvider &operator=(const PtpTimeBaseProvider &) = delete;

            core::Result<void> UpdateTimeBase(TimeSyncClient &client) override;
            bool IsSourceAvailable() const override;
            const char *GetProviderName() const noexcept override;

            /// @brief Get the offset between PHC and system clock.
            /// @returns Offset in nanoseconds (PHC - system), or error.
            core::Result<std::chrono::nanoseconds> GetPtpOffset() const;

            /// @brief Get the configured PTP device path.
            const std::string &GetDevicePath() const noexcept;

        protected:
            /// @brief Read the PHC clock and compute offset vs system clock.
            /// @note Protected virtual for testability.
            virtual core::Result<std::chrono::nanoseconds> ReadPtpClock() const;

        private:
            std::string mPtpDevice;
            int mFd;

            bool OpenDevice();
            void CloseDevice();
        };
    }
}

#endif
