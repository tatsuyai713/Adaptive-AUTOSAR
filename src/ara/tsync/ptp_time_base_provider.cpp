/// @file src/ara/tsync/ptp_time_base_provider.cpp
/// @brief Implementation for PTP/gPTP time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./ptp_time_base_provider.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

/// PHC clock ID conversion macro (Linux kernel convention).
#ifndef CLOCKFD
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)
#endif

namespace ara
{
    namespace tsync
    {
        PtpTimeBaseProvider::PtpTimeBaseProvider(const std::string &ptpDevice)
            : mPtpDevice{ptpDevice},
              mFd{-1}
        {
            OpenDevice();
        }

        PtpTimeBaseProvider::~PtpTimeBaseProvider() noexcept
        {
            CloseDevice();
        }

        bool PtpTimeBaseProvider::OpenDevice()
        {
            if (mFd >= 0)
            {
                return true;
            }
            mFd = ::open(mPtpDevice.c_str(), O_RDONLY);
            return mFd >= 0;
        }

        void PtpTimeBaseProvider::CloseDevice()
        {
            if (mFd >= 0)
            {
                ::close(mFd);
                mFd = -1;
            }
        }

        core::Result<std::chrono::nanoseconds>
        PtpTimeBaseProvider::ReadPtpClock() const
        {
            if (mFd < 0)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kDeviceOpenFailed));
            }

            clockid_t _clkId = FD_TO_CLOCKID(mFd);
            struct timespec _tsPtp;
            struct timespec _tsSys;

            if (clock_gettime(_clkId, &_tsPtp) != 0)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            if (clock_gettime(CLOCK_REALTIME, &_tsSys) != 0)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            const int64_t _offsetNs =
                (static_cast<int64_t>(_tsPtp.tv_sec) -
                 static_cast<int64_t>(_tsSys.tv_sec)) *
                    1000000000LL +
                (static_cast<int64_t>(_tsPtp.tv_nsec) -
                 static_cast<int64_t>(_tsSys.tv_nsec));

            return core::Result<std::chrono::nanoseconds>::FromValue(
                std::chrono::nanoseconds{_offsetNs});
        }

        core::Result<std::chrono::nanoseconds>
        PtpTimeBaseProvider::GetPtpOffset() const
        {
            return ReadPtpClock();
        }

        core::Result<void> PtpTimeBaseProvider::UpdateTimeBase(
            TimeSyncClient &client)
        {
            auto _offsetResult = ReadPtpClock();
            if (!_offsetResult.HasValue())
            {
                return core::Result<void>::FromError(_offsetResult.Error());
            }

            const auto _steadyNow = std::chrono::steady_clock::now();
            const auto _systemNow = std::chrono::system_clock::now();
            const auto _corrected = _systemNow +
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    _offsetResult.Value());

            return client.UpdateReferenceTime(_corrected, _steadyNow);
        }

        bool PtpTimeBaseProvider::IsSourceAvailable() const
        {
            if (mFd < 0)
            {
                return false;
            }

            clockid_t _clkId = FD_TO_CLOCKID(mFd);
            struct timespec _ts;
            return clock_gettime(_clkId, &_ts) == 0;
        }

        const char *PtpTimeBaseProvider::GetProviderName() const noexcept
        {
            return "PTP/gPTP";
        }

        const std::string &PtpTimeBaseProvider::GetDevicePath() const noexcept
        {
            return mPtpDevice;
        }
    }
}
