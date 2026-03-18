/// @file src/ara/tsync/rate_corrector.cpp
/// @brief Implementation of RateCorrector.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./rate_corrector.h"
#include <cmath>
#if defined(__linux__) || defined(__QNX__)
#include <sys/timex.h>
#endif

namespace ara
{
    namespace tsync
    {
        RateCorrector::RateCorrector(const RateCorrectorConfig &config)
            : mConfig{config},
              mIntegral{0.0},
              mPrevError{0.0},
              mCorrectionPpb{0.0},
              mSampleCount{0}
        {
            mHistory.reserve(mConfig.WindowSize);
        }

        core::Result<double> RateCorrector::FeedOffset(double offsetNs)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            // Maintain sliding window.
            if (mHistory.size() >= mConfig.WindowSize)
            {
                mHistory.erase(mHistory.begin());
            }
            mHistory.push_back(offsetNs);
            ++mSampleCount;

            // Compute windowed average offset as the error signal.
            double sum = 0.0;
            for (auto v : mHistory) sum += v;
            double error = sum / static_cast<double>(mHistory.size());

            // PID computation.
            mIntegral += error;
            double derivative = error - mPrevError;
            mPrevError = error;

            double correction =
                mConfig.Kp * error +
                mConfig.Ki * mIntegral +
                mConfig.Kd * derivative;

            mCorrectionPpb = Clamp(correction, mConfig.MaxCorrectionPpb);

#if defined(__linux__) || defined(__QNX__)
            // Apply the computed frequency correction to the kernel clock.
            // Kernel ADJ_FREQUENCY unit: parts-per-million scaled by 2^16.
            // 1 ppb = 1e-3 ppm, so kernel_freq = ppb * 65536 / 1000.
            struct timex tx{};
            tx.modes = ADJ_FREQUENCY;
            tx.freq = static_cast<long>(mCorrectionPpb * 65.536);
            ::adjtimex(&tx);
#endif

            return mCorrectionPpb;
        }

        double RateCorrector::GetCorrectionPpb() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCorrectionPpb;
        }

        CorrectorState RateCorrector::GetState() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            CorrectorState state;
            state.LastOffsetNs = mHistory.empty() ? 0.0 : mHistory.back();
            state.CorrectionPpb = mCorrectionPpb;
            state.IntegralAccumulator = mIntegral;
            state.SampleCount = mSampleCount;
            // Consider locked when window is full and correction is small.
            state.Locked =
                (mHistory.size() >= mConfig.WindowSize) &&
                (std::abs(mCorrectionPpb) < mConfig.MaxCorrectionPpb * 0.1);
            return state;
        }

        void RateCorrector::Reset()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mHistory.clear();
            mIntegral = 0.0;
            mPrevError = 0.0;
            mCorrectionPpb = 0.0;
            mSampleCount = 0;
        }

        bool RateCorrector::IsLocked() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return (mHistory.size() >= mConfig.WindowSize) &&
                   (std::abs(mCorrectionPpb) <
                    mConfig.MaxCorrectionPpb * 0.1);
        }

        double RateCorrector::Clamp(double val, double limit) const
        {
            if (val > limit) return limit;
            if (val < -limit) return -limit;
            return val;
        }
    }
}
