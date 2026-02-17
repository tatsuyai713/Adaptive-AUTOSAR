/// @file src/ara/core/steady_clock.h
/// @brief ara::core::SteadyClock — AUTOSAR AP SWS_CORE monotonic clock type.
/// @details Provides ara::core::SteadyClock as a wrapper around
///          std::chrono::steady_clock, conforming to AUTOSAR Adaptive Platform
///          Core specification (SWS_CORE_06401).
///
///          SteadyClock is a monotonic clock (never goes backward) suitable for
///          measuring elapsed time, timeouts, and interval scheduling.
///          It is NOT affected by system time adjustments (NTP, PTP, etc.).
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_STEADY_CLOCK_H
#define ARA_CORE_STEADY_CLOCK_H

#include <chrono>
#include <cstdint>

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP monotonic clock (SWS_CORE_06401).
        /// @details Thin wrapper around std::chrono::steady_clock.
        ///          Use this type when you need elapsed time measurements that
        ///          are unaffected by wall-clock changes (NTP steps, PTP sync).
        ///
        /// @note To get the current time synchronized to PTP/gPTP, use
        ///       ara::tsync::TimeSyncClient or ara::tsync::PtpTimeBaseProvider instead.
        struct SteadyClock
        {
            /// @brief Duration type (nanoseconds)
            using duration   = std::chrono::nanoseconds;

            /// @brief Time point type
            using time_point = std::chrono::time_point<SteadyClock, duration>;

            /// @brief Period type (nanosecond ratio)
            using period     = duration::period;

            /// @brief Representation type (64-bit signed integer)
            using rep        = duration::rep;

            /// @brief The clock is steady (monotonic) — always true
            static constexpr bool is_steady = true;

            /// @brief Get the current time point.
            /// @returns Current monotonic time as SteadyClock::time_point.
            static time_point now() noexcept
            {
                const auto tp = std::chrono::steady_clock::now();
                const auto ns = std::chrono::duration_cast<duration>(tp.time_since_epoch());
                return time_point(ns);
            }

            /// @brief Get current time as nanoseconds since epoch.
            /// @returns Nanoseconds since the steady clock epoch (process start or boot).
            static int64_t NowNanoseconds() noexcept
            {
                return now().time_since_epoch().count();
            }

            /// @brief Get current time as microseconds since epoch.
            static int64_t NowMicroseconds() noexcept
            {
                return std::chrono::duration_cast<std::chrono::microseconds>(
                           now().time_since_epoch())
                    .count();
            }

            /// @brief Get current time as milliseconds since epoch.
            static int64_t NowMilliseconds() noexcept
            {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                           now().time_since_epoch())
                    .count();
            }

            /// @brief Compute elapsed nanoseconds between two time points.
            static int64_t ElapsedNanoseconds(time_point from, time_point to) noexcept
            {
                return std::chrono::duration_cast<duration>(to - from).count();
            }
        };

    } // namespace core
} // namespace ara

#endif // ARA_CORE_STEADY_CLOCK_H
