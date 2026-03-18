#include <gtest/gtest.h>
#include "../../../src/ara/nm/cantp_nm_adapter.h"

namespace ara
{
    namespace nm
    {
        TEST(CanTpNmAdapterTest, DefaultConstruction)
        {
            CanNmConfig _config;
            _config.InterfaceName = "vcan0";
            _config.BaseCanId = 0x400;
            CanTpNmAdapter _adapter{_config};
            EXPECT_EQ(_adapter.GetSentCount(), 0U);
            EXPECT_EQ(_adapter.GetReceivedCount(), 0U);
        }

        TEST(CanTpNmAdapterTest, MapChannelSucceeds)
        {
            CanNmConfig _config;
            _config.InterfaceName = "vcan0";
            CanTpNmAdapter _adapter{_config};
            // MapChannel returns void; no crash = success
            _adapter.MapChannel("ch1", 0x10);
        }

        TEST(CanTpNmAdapterTest, MapDuplicateChannelNoThrow)
        {
            CanNmConfig _config;
            _config.InterfaceName = "vcan0";
            CanTpNmAdapter _adapter{_config};
            _adapter.MapChannel("ch1", 0x10);
            // Second map with same name — implementation silently ignores
            _adapter.MapChannel("ch1", 0x20);
        }

        TEST(CanTpNmAdapterTest, SendWithoutSocketDoesNotCrash)
        {
            CanNmConfig _config;
            _config.InterfaceName = "vcan0";
            CanTpNmAdapter _adapter{_config};
            _adapter.MapChannel("ch1", 0x10);
            // Without an actual socket, this is a no-op
            auto _result = _adapter.SendNmPdu("ch1", {0x01, 0x02, 0x03});
            // May fail (no socket) — just verify no crash
            (void)_result;
        }

        TEST(CanTpNmAdapterTest, ConfigFields)
        {
            CanNmConfig _config;
            _config.InterfaceName = "can0";
            _config.BaseCanId = 0x600;
            _config.UseExtendedId = true;
            _config.MaxPayload = 64;
            _config.PollTimeoutMs = 100;
            EXPECT_EQ(_config.InterfaceName, "can0");
            EXPECT_EQ(_config.BaseCanId, 0x600U);
            EXPECT_TRUE(_config.UseExtendedId);
            EXPECT_EQ(_config.MaxPayload, 64U);
            EXPECT_EQ(_config.PollTimeoutMs, 100U);
        }
    }
}
