#include <gtest/gtest.h>
#include "../../../src/ara/sm/network_interface_controller.h"

namespace ara
{
    namespace sm
    {
        TEST(NetworkInterfaceControllerTest, RegisterInterface)
        {
            NetworkInterfaceController _nic;
            auto _result = _nic.RegisterInterface("eth0");
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, RegisterDuplicate)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("eth0");
            auto _result = _nic.RegisterInterface("eth0");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, EnableInterface)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("aatest0");
            auto _result = _nic.EnableInterface("aatest0");
#if defined(__linux__) || defined(__QNX__)
            EXPECT_FALSE(_result.HasValue());
            auto _enabled = _nic.IsEnabled("aatest0");
            ASSERT_TRUE(_enabled.HasValue());
            EXPECT_FALSE(_enabled.Value());
#else
            EXPECT_TRUE(_result.HasValue());
#endif
        }

        TEST(NetworkInterfaceControllerTest, EnableUnregistered)
        {
            NetworkInterfaceController _nic;
            auto _result = _nic.EnableInterface("eth0");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, DisableInterface)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("aatest0");
            auto _result = _nic.DisableInterface("aatest0");
#if defined(__linux__) || defined(__QNX__)
            EXPECT_FALSE(_result.HasValue());
#else
            EXPECT_TRUE(_result.HasValue());
#endif
        }

        TEST(NetworkInterfaceControllerTest, SetMtu)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("aatest0");
            auto _result = _nic.SetMtu("aatest0", 9000);
#if defined(__linux__) || defined(__QNX__)
            EXPECT_FALSE(_result.HasValue());
            auto _info = _nic.GetInterfaceInfo("aatest0");
            ASSERT_TRUE(_info.HasValue());
            EXPECT_EQ(_info.Value().Mtu, 1500U);
#else
            EXPECT_TRUE(_result.HasValue());
#endif
        }

        TEST(NetworkInterfaceControllerTest, SetMtuInvalidSize)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("eth0");
            auto _result = _nic.SetMtu("eth0", 0);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, SetIpv4Address)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("aatest0");
            auto _result = _nic.SetIpv4Address("aatest0", "192.168.1.100");
#if defined(__linux__) || defined(__QNX__)
            EXPECT_FALSE(_result.HasValue());
            auto _info = _nic.GetInterfaceInfo("aatest0");
            ASSERT_TRUE(_info.HasValue());
            EXPECT_TRUE(_info.Value().Ipv4Address.empty());
#else
            EXPECT_TRUE(_result.HasValue());
#endif
        }

        TEST(NetworkInterfaceControllerTest, SetIpv4AddressRejectsInvalidInput)
        {
            NetworkInterfaceController _nic;
            ASSERT_TRUE(_nic.RegisterInterface("aatest0").HasValue());

            auto _result = _nic.SetIpv4Address("aatest0", "not-an-ip");

            EXPECT_FALSE(_result.HasValue());
            auto _info = _nic.GetInterfaceInfo("aatest0");
            ASSERT_TRUE(_info.HasValue());
            EXPECT_TRUE(_info.Value().Ipv4Address.empty());
        }

        TEST(NetworkInterfaceControllerTest, RejectsTooLongInterfaceName)
        {
            NetworkInterfaceController _nic;

            auto _result = _nic.RegisterInterface("autosar_test_interface_name_too_long");

#if defined(__linux__) || defined(__QNX__)
            EXPECT_FALSE(_result.HasValue());
#else
            EXPECT_TRUE(_result.HasValue());
#endif
        }

        TEST(NetworkInterfaceControllerTest, GetInfoAfterChanges)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("eth0");
            auto _info = _nic.GetInterfaceInfo("eth0");
            ASSERT_TRUE(_info.HasValue());
            EXPECT_EQ(_info.Value().Name, "eth0");
            EXPECT_EQ(_info.Value().Status, LinkStatus::kUnknown);
            EXPECT_EQ(_info.Value().Mtu, 1500U);
            EXPECT_TRUE(_info.Value().Ipv4Address.empty());
        }
    }
}
