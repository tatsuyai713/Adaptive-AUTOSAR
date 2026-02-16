/// @file src/ara/tsync/ntp_time_base_provider.cpp
/// @brief Implementation for NTP time base provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./ntp_time_base_provider.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace ara
{
    namespace tsync
    {
        NtpTimeBaseProvider::NtpTimeBaseProvider(NtpDaemon daemon)
            : mConfiguredDaemon{daemon},
              mDetectedDaemon{daemon}
        {
        }

        core::Result<std::string> NtpTimeBaseProvider::RunCommand(
            const std::string &cmd) const
        {
            FILE *_pipe = popen(cmd.c_str(), "r");
            if (_pipe == nullptr)
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            std::string _output;
            char _buffer[256];
            while (std::fgets(_buffer, sizeof(_buffer), _pipe) != nullptr)
            {
                _output += _buffer;
            }

            int _status = pclose(_pipe);
            if (_status != 0 || _output.empty())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            return core::Result<std::string>::FromValue(std::move(_output));
        }

        core::Result<std::chrono::nanoseconds>
        NtpTimeBaseProvider::ParseChronyOutput(const std::string &output)
        {
            // chronyc -c tracking CSV format:
            // field 0: refid (hex)
            // field 1: refid (name)
            // field 2: stratum
            // field 3: ref time (UTC seconds.fraction)
            // field 4: system time offset (seconds, signed float)
            // ...
            // We need field index 4 (0-based).

            std::istringstream _stream{output};
            std::string _field;
            int _index = 0;

            while (std::getline(_stream, _field, ','))
            {
                if (_index == 4)
                {
                    char *_endPtr = nullptr;
                    double _offsetSec = std::strtod(_field.c_str(), &_endPtr);
                    if (_endPtr == _field.c_str() || _field.empty())
                    {
                        return core::Result<std::chrono::nanoseconds>::FromError(
                            MakeErrorCode(TsyncErrc::kQueryFailed));
                    }

                    const int64_t _offsetNs =
                        static_cast<int64_t>(_offsetSec * 1e9);
                    return core::Result<std::chrono::nanoseconds>::FromValue(
                        std::chrono::nanoseconds{_offsetNs});
                }
                ++_index;
            }

            return core::Result<std::chrono::nanoseconds>::FromError(
                MakeErrorCode(TsyncErrc::kQueryFailed));
        }

        core::Result<std::chrono::nanoseconds>
        NtpTimeBaseProvider::ParseNtpqOutput(const std::string &output)
        {
            // ntpq -c rv 0 output contains key=value pairs.
            // We look for "offset=<value>" where value is in milliseconds.
            const std::string cKey{"offset="};
            auto _pos = output.find(cKey);
            if (_pos == std::string::npos)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            _pos += cKey.size();
            const char *_start = output.c_str() + _pos;
            char *_endPtr = nullptr;
            double _offsetMs = std::strtod(_start, &_endPtr);
            if (_endPtr == _start)
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kQueryFailed));
            }

            // Convert milliseconds to nanoseconds.
            const int64_t _offsetNs =
                static_cast<int64_t>(_offsetMs * 1e6);
            return core::Result<std::chrono::nanoseconds>::FromValue(
                std::chrono::nanoseconds{_offsetNs});
        }

        NtpDaemon NtpTimeBaseProvider::DetectDaemon() const
        {
            if (mConfiguredDaemon != NtpDaemon::kAuto)
            {
                return mConfiguredDaemon;
            }

            // Try chrony first.
            auto _chronyResult = RunCommand("chronyc -c tracking 2>/dev/null");
            if (_chronyResult.HasValue())
            {
                return NtpDaemon::kChrony;
            }

            // Try ntpd.
            auto _ntpqResult = RunCommand("ntpq -c rv 0 2>/dev/null");
            if (_ntpqResult.HasValue())
            {
                return NtpDaemon::kNtpd;
            }

            return NtpDaemon::kAuto; // None detected.
        }

        core::Result<std::chrono::nanoseconds>
        NtpTimeBaseProvider::QueryChronyOffset() const
        {
            auto _result = RunCommand("chronyc -c tracking 2>/dev/null");
            if (!_result.HasValue())
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    _result.Error());
            }
            return ParseChronyOutput(_result.Value());
        }

        core::Result<std::chrono::nanoseconds>
        NtpTimeBaseProvider::QueryNtpdOffset() const
        {
            auto _result = RunCommand("ntpq -c rv 0 2>/dev/null");
            if (!_result.HasValue())
            {
                return core::Result<std::chrono::nanoseconds>::FromError(
                    _result.Error());
            }
            return ParseNtpqOutput(_result.Value());
        }

        core::Result<std::chrono::nanoseconds>
        NtpTimeBaseProvider::GetNtpOffset() const
        {
            NtpDaemon _daemon = DetectDaemon();
            mDetectedDaemon = _daemon;

            switch (_daemon)
            {
            case NtpDaemon::kChrony:
                return QueryChronyOffset();
            case NtpDaemon::kNtpd:
                return QueryNtpdOffset();
            default:
                return core::Result<std::chrono::nanoseconds>::FromError(
                    MakeErrorCode(TsyncErrc::kProviderUnavailable));
            }
        }

        core::Result<void> NtpTimeBaseProvider::UpdateTimeBase(
            TimeSyncClient &client)
        {
            auto _offsetResult = GetNtpOffset();
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

        bool NtpTimeBaseProvider::IsSourceAvailable() const
        {
            NtpDaemon _daemon = DetectDaemon();
            return _daemon != NtpDaemon::kAuto;
        }

        const char *NtpTimeBaseProvider::GetProviderName() const noexcept
        {
            return "NTP";
        }

        NtpDaemon NtpTimeBaseProvider::GetActiveDaemon() const noexcept
        {
            return mDetectedDaemon;
        }
    }
}
