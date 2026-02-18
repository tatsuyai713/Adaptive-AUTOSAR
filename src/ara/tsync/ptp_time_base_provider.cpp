/// @file src/ara/tsync/ptp_time_base_provider.cpp
/// @brief Implementation for PTP/gPTP time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./ptp_time_base_provider.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Platform-specific PHC access
// ---------------------------------------------------------------------------
// Linux: use the FD_TO_CLOCKID() kernel convention (dynamic clockid from fd).
// QNX:  FD_TO_CLOCKID() is not supported.  Use PTP_SYS_OFFSET ioctl instead,
//       which returns paired (system, PHC, system) timestamps in a single call.
//       If the ioctl is unavailable (ENOTTY / ENOTSUP), fall back to offset=0
//       — this is correct when a PTP daemon disciplines CLOCK_REALTIME.
// ---------------------------------------------------------------------------

#if defined(__QNX__)

#include <cerrno>
#include <cstdint>
#include <sys/ioctl.h>

/// Maximum samples accepted by PTP_SYS_OFFSET ioctl.
static constexpr unsigned int kPtpMaxSamples = 25U;

/// Single PTP clock timestamp (matches linux/ptp_clock.h ptp_clock_time).
struct PtpClockTime
{
    std::int64_t  sec;
    std::uint32_t nsec;
    std::uint32_t reserved;
};

/// Request struct for PTP_SYS_OFFSET ioctl.
/// Layout is identical to linux/ptp_clock.h ptp_sys_offset.
struct PtpSysOffset
{
    unsigned int n_samples;
    unsigned int rsv[3];
    PtpClockTime ts[2U * kPtpMaxSamples + 1U]; ///< [sys, phc, sys, ...]
};

/// ioctl command — encoding matches Linux _IOWR('=', 5, struct ptp_sys_offset).
/// QNX uses the same _IOC encoding, so the numeric value is identical.
#define PTP_SYS_OFFSET _IOWR('=', 5, struct PtpSysOffset)

namespace
{
    /// Read PHC-to-system offset via PTP_SYS_OFFSET ioctl.
    /// Returns nanoseconds or a flag value INT64_MIN when unsupported.
    int64_t ReadPhcOffsetQnx(int fd)
    {
        PtpSysOffset _req{};
        _req.n_samples = 1U;

        if (::ioctl(fd, PTP_SYS_OFFSET, &_req) != 0)
        {
            // Device exists but ioctl not supported — offset unknown.
            return INT64_MIN;
        }

        // ts[0] = sys_before, ts[1] = phc, ts[2] = sys_after
        const int64_t _sysBefore =
            _req.ts[0].sec * 1000000000LL + static_cast<int64_t>(_req.ts[0].nsec);
        const int64_t _phc =
            _req.ts[1].sec * 1000000000LL + static_cast<int64_t>(_req.ts[1].nsec);
        const int64_t _sysAfter =
            _req.ts[2].sec * 1000000000LL + static_cast<int64_t>(_req.ts[2].nsec);

        const int64_t _sysMid = (_sysBefore + _sysAfter) / 2LL;
        return _phc - _sysMid;
    }
} // namespace

#else // Linux

/// PHC clock ID conversion macro (Linux kernel convention).
#ifndef CLOCKFD
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)
#endif

#endif // __QNX__

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

#if defined(__QNX__)
            // QNX: use PTP_SYS_OFFSET ioctl for paired PHC/system timestamps.
            const int64_t _offsetNs = ReadPhcOffsetQnx(mFd);
            if (_offsetNs == INT64_MIN)
            {
                // ioctl unsupported — assume PTP daemon disciplines CLOCK_REALTIME.
                return core::Result<std::chrono::nanoseconds>::FromValue(
                    std::chrono::nanoseconds{0});
            }
            return core::Result<std::chrono::nanoseconds>::FromValue(
                std::chrono::nanoseconds{_offsetNs});
#else
            // Linux: derive clockid_t from the PHC file descriptor.
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
#endif
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

#if defined(__QNX__)
            // On QNX the fd being open means the PHC device exists.
            // Attempt the ioctl to confirm readability; fall back to true on ENOTTY.
            PtpSysOffset _req{};
            _req.n_samples = 1U;
            if (::ioctl(mFd, PTP_SYS_OFFSET, &_req) == 0)
            {
                return true;
            }
            // ENOTTY / ENOTSUP: device open but ioctl not supported — treat as available
            // (offset=0 fallback will be used in ReadPtpClock).
            return (errno == ENOTTY || errno == ENOTSUP);
#else
            clockid_t _clkId = FD_TO_CLOCKID(mFd);
            struct timespec _ts;
            return clock_gettime(_clkId, &_ts) == 0;
#endif
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
