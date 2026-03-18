#include <gtest/gtest.h>
#include "../../../src/ara/nm/flexray_nm_adapter.h"

namespace ara
{
    namespace nm
    {
        TEST(FlexRayNmAdapterTest, DefaultConstruction)
        {
            FlexRayNmConfig _config;
            FlexRayNmAdapter _adapter{_config};
            EXPECT_EQ(_adapter.GetSentCount(), 0U);
            EXPECT_EQ(_adapter.GetReceivedCount(), 0U);
        }

        TEST(FlexRayNmAdapterTest, MapChannelSucceeds)
        {
            FlexRayNmConfig _config;
            FlexRayNmAdapter _adapter{_config};
            FlexRayNmSlot _slot;
            _slot.NmVotingSlot = 1;
            _slot.NmDataSlot = 2;
            _adapter.MapChannel("ch1", _slot);
        }

        TEST(FlexRayNmAdapterTest, InjectReceivedPdu)
        {
            FlexRayNmConfig _config;
            FlexRayNmAdapter _adapter{_config};
            FlexRayNmSlot _slot;
            _adapter.MapChannel("ch1", _slot);

            _adapter.InjectReceivedPdu("ch1", {0xAA, 0xBB});
            EXPECT_EQ(_adapter.GetReceivedCount(), 1U);
        }

        TEST(FlexRayNmAdapterTest, TimingConfig)
        {
            FlexRayTimingConfig _timing;
            EXPECT_EQ(_timing.CyclePeriodUs, 5000U);
            EXPECT_EQ(_timing.StaticSlotCount, 64U);
            EXPECT_EQ(_timing.StaticSlotDurationUs, 50U);
            EXPECT_EQ(_timing.StaticPayloadBytes, 32U);
        }

        TEST(FlexRayNmAdapterTest, SendNmPduQueues)
        {
            FlexRayNmConfig _config;
            FlexRayNmAdapter _adapter{_config};
            FlexRayNmSlot _slot;
            _adapter.MapChannel("ch1", _slot);

            auto _result = _adapter.SendNmPdu("ch1", {0x01});
            EXPECT_TRUE(_result.HasValue());
        }
    }
}
