#include <gtest/gtest.h>
#include "../../../src/ara/tsync/rate_corrector.h"

namespace ara
{
    namespace tsync
    {
        TEST(RateCorrectorTest, InitialState)
        {
            RateCorrector _rc;
            EXPECT_FALSE(_rc.IsLocked());
            EXPECT_DOUBLE_EQ(_rc.GetCorrectionPpb(), 0.0);
            auto _state = _rc.GetState();
            EXPECT_EQ(_state.SampleCount, 0U);
            EXPECT_FALSE(_state.Locked);
        }

        TEST(RateCorrectorTest, FeedOffsetReturnsResult)
        {
            RateCorrector _rc;
            auto _r = _rc.FeedOffset(100.0);
            EXPECT_TRUE(_r.HasValue());
            // Correction should be non-zero after a positive offset
            EXPECT_NE(_r.Value(), 0.0);
        }

        TEST(RateCorrectorTest, MultipleSamplesConverge)
        {
            RateCorrector _rc;
            for (int i = 0; i < 20; ++i)
            {
                _rc.FeedOffset(50.0);
            }
            auto _state = _rc.GetState();
            EXPECT_EQ(_state.SampleCount, 20U);
            EXPECT_NE(_state.CorrectionPpb, 0.0);
        }

        TEST(RateCorrectorTest, ResetClearsState)
        {
            RateCorrector _rc;
            _rc.FeedOffset(100.0);
            _rc.Reset();
            EXPECT_DOUBLE_EQ(_rc.GetCorrectionPpb(), 0.0);
            auto _state = _rc.GetState();
            EXPECT_EQ(_state.SampleCount, 0U);
        }

        TEST(RateCorrectorTest, CustomConfig)
        {
            RateCorrectorConfig _cfg;
            _cfg.Kp = 1.0;
            _cfg.Ki = 0.0;
            _cfg.Kd = 0.0;
            _cfg.MaxCorrectionPpb = 500.0;
            RateCorrector _rc(_cfg);
            auto _r = _rc.FeedOffset(1000.0);
            EXPECT_TRUE(_r.HasValue());
            // Should be clamped to MaxCorrectionPpb
            EXPECT_LE(std::abs(_r.Value()), 500.0);
        }

        TEST(RateCorrectorTest, NegativeOffset)
        {
            RateCorrector _rc;
            auto _r = _rc.FeedOffset(-200.0);
            EXPECT_TRUE(_r.HasValue());
            // Should produce a negative correction
            EXPECT_LT(_r.Value(), 0.0);
        }

        TEST(RateCorrectorTest, ZeroOffsetProducesZero)
        {
            RateCorrector _rc;
            auto _r = _rc.FeedOffset(0.0);
            EXPECT_TRUE(_r.HasValue());
            EXPECT_DOUBLE_EQ(_r.Value(), 0.0);
        }
    }
}
