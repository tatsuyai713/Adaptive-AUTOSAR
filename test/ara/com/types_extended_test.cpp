#include <gtest/gtest.h>
#include "../../../src/ara/com/types.h"

namespace ara
{
    namespace com
    {
        TEST(EventCacheUpdatePolicyTest, EnumValues)
        {
            EXPECT_EQ(
                static_cast<uint8_t>(EventCacheUpdatePolicy::kLastN), 0U);
            EXPECT_EQ(
                static_cast<uint8_t>(EventCacheUpdatePolicy::kNewestN), 1U);
        }

        TEST(FilterTypeTest, EnumValues)
        {
            EXPECT_EQ(static_cast<uint8_t>(FilterType::kNone), 0U);
            EXPECT_EQ(static_cast<uint8_t>(FilterType::kThreshold), 1U);
            EXPECT_EQ(static_cast<uint8_t>(FilterType::kRange), 2U);
            EXPECT_EQ(static_cast<uint8_t>(FilterType::kOneEveryN), 3U);
        }

        TEST(FilterConfigTest, NoneFactory)
        {
            auto cfg = FilterConfig::None();
            EXPECT_EQ(cfg.Type, FilterType::kNone);
        }

        TEST(FilterConfigTest, ThresholdFactory)
        {
            auto cfg = FilterConfig::Threshold(10.5);
            EXPECT_EQ(cfg.Type, FilterType::kThreshold);
            EXPECT_DOUBLE_EQ(cfg.ThresholdValue, 10.5);
        }

        TEST(FilterConfigTest, RangeFactory)
        {
            auto cfg = FilterConfig::Range(1.0, 100.0);
            EXPECT_EQ(cfg.Type, FilterType::kRange);
            EXPECT_DOUBLE_EQ(cfg.RangeLow, 1.0);
            EXPECT_DOUBLE_EQ(cfg.RangeHigh, 100.0);
        }

        TEST(FilterConfigTest, OneEveryNFactory)
        {
            auto cfg = FilterConfig::OneEveryN(5);
            EXPECT_EQ(cfg.Type, FilterType::kOneEveryN);
            EXPECT_EQ(cfg.DecimationFactor, 5U);
        }

        TEST(FilterConfigTest, OneEveryNZeroClamp)
        {
            auto cfg = FilterConfig::OneEveryN(0);
            EXPECT_EQ(cfg.DecimationFactor, 1U);
        }

        TEST(FilterConfigTest, DefaultConstruction)
        {
            FilterConfig cfg;
            EXPECT_EQ(cfg.Type, FilterType::kNone);
            EXPECT_DOUBLE_EQ(cfg.ThresholdValue, 0.0);
            EXPECT_DOUBLE_EQ(cfg.RangeLow, 0.0);
            EXPECT_DOUBLE_EQ(cfg.RangeHigh, 0.0);
            EXPECT_EQ(cfg.DecimationFactor, 1U);
        }

        TEST(SizedEventReceiveHandlerTest, Callable)
        {
            std::size_t captured = 0U;
            SizedEventReceiveHandler handler =
                [&captured](std::size_t count)
            {
                captured = count;
            };
            handler(42);
            EXPECT_EQ(captured, 42U);
        }

        TEST(SizedEventReceiveHandlerTest, Null)
        {
            SizedEventReceiveHandler handler;
            EXPECT_FALSE(static_cast<bool>(handler));
        }
    }
}
