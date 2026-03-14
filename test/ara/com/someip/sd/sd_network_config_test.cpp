#include <gtest/gtest.h>
#include "../../../../../src/ara/com/someip/sd/sd_network_config.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                TEST(SdNetworkConfigTest, DefaultValues)
                {
                    SdNetworkConfig config;
                    EXPECT_EQ(config.MulticastAddress, "239.0.0.1");
                    EXPECT_EQ(config.MulticastPort, 30490U);
                    EXPECT_EQ(config.MulticastTtl, 1U);
                    EXPECT_EQ(config.MaxEntriesPerMessage, 0U);
                    EXPECT_TRUE(config.RebootDetection);
                }

                TEST(SdNetworkConfigTest, CustomMulticastAddress)
                {
                    SdNetworkConfig config;
                    config.MulticastAddress = "239.1.2.3";
                    EXPECT_EQ(config.MulticastAddress, "239.1.2.3");
                }

                TEST(SdNetworkConfigTest, CustomTtl)
                {
                    SdNetworkConfig config;
                    config.MulticastTtl = 64U;
                    EXPECT_EQ(config.MulticastTtl, 64U);
                }

                TEST(SdNetworkConfigTest, CustomPort)
                {
                    SdNetworkConfig config;
                    config.MulticastPort = 31000U;
                    EXPECT_EQ(config.MulticastPort, 31000U);
                }

                TEST(SdNetworkConfigTest, ValidateDefaultConfig)
                {
                    SdNetworkConfig config;
                    EXPECT_TRUE(ValidateSdNetworkConfig(config));
                }

                TEST(SdNetworkConfigTest, ValidateZeroTtlFails)
                {
                    SdNetworkConfig config;
                    config.MulticastTtl = 0U;
                    EXPECT_FALSE(ValidateSdNetworkConfig(config));
                }

                TEST(SdNetworkConfigTest, ValidateZeroPortFails)
                {
                    SdNetworkConfig config;
                    config.MulticastPort = 0U;
                    EXPECT_FALSE(ValidateSdNetworkConfig(config));
                }

                TEST(SdNetworkConfigTest, MaxEntriesPerMessage)
                {
                    SdNetworkConfig config;
                    config.MaxEntriesPerMessage = 10U;
                    EXPECT_EQ(config.MaxEntriesPerMessage, 10U);
                    EXPECT_TRUE(ValidateSdNetworkConfig(config));
                }

                TEST(SdNetworkConfigTest, DisableRebootDetection)
                {
                    SdNetworkConfig config;
                    config.RebootDetection = false;
                    EXPECT_FALSE(config.RebootDetection);
                    EXPECT_TRUE(ValidateSdNetworkConfig(config));
                }
            }
        }
    }
}
