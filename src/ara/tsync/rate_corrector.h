/// @file src/ara/tsync/rate_corrector.h
/// @brief Active clock rate correction (PID-style discipliner).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef TSYNC_RATE_CORRECTOR_H
#define TSYNC_RATE_CORRECTOR_H

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>
#include "../core/result.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        /// @brief PID controller tuning parameters for clock discipline.
        struct RateCorrectorConfig
        {
            double Kp{0.5};   ///< Proportional gain.
            double Ki{0.01};  ///< Integral gain.
            double Kd{0.1};   ///< Derivative gain.
            double MaxCorrectionPpb{1000.0}; ///< Max adjustment in ppb.
            uint32_t WindowSize{16};         ///< Sliding window size.
        };

        /// @brief Snapshot of the corrector state.
        struct CorrectorState
        {
            double LastOffsetNs{0.0};
            double CorrectionPpb{0.0};
            double IntegralAccumulator{0.0};
            uint32_t SampleCount{0};
            bool Locked{false};
        };

        /// @brief Active clock rate corrector using a PID controller.
        /// @details Accepts periodic offset measurements (local vs reference)
        ///          and computes a frequency correction in ppb to discipline
        ///          the local oscillator.
        class RateCorrector
        {
        public:
            explicit RateCorrector(
                const RateCorrectorConfig &config = RateCorrectorConfig{});
            ~RateCorrector() = default;

            /// @brief Feed a new offset sample (reference − local), in ns.
            /// @returns The updated correction in ppb.
            core::Result<double> FeedOffset(double offsetNs);

            /// @brief Get the current correction value in ppb.
            double GetCorrectionPpb() const;

            /// @brief Get full corrector state.
            CorrectorState GetState() const;

            /// @brief Reset the corrector to its initial state.
            void Reset();

            /// @brief Check if the corrector is locked (offset converged).
            bool IsLocked() const;

        private:
            mutable std::mutex mMutex;
            RateCorrectorConfig mConfig;
            std::vector<double> mHistory;
            double mIntegral{0.0};
            double mPrevError{0.0};
            double mCorrectionPpb{0.0};
            uint32_t mSampleCount{0};

            double Clamp(double val, double limit) const;
        };
    }
}

#endif
