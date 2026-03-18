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
            _nic.RegisterInterface("eth0");
            auto _result = _nic.EnableInterface("eth0");
            EXPECT_TRUE(_result.HasValue());
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
            _nic.RegisterInterface("eth0");
            _nic.EnableInterface("eth0");
            auto _result = _nic.DisableInterface("eth0");
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, SetMtu)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("eth0");
            auto _result = _nic.SetMtu("eth0", 1500);
            EXPECT_TRUE(_result.HasValue());
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
            _nic.RegisterInterface("eth0");
            auto _result = _nic.SetIpv4Address("eth0", "192.168.1.100");
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(NetworkInterfaceControllerTest, GetInfoAfterChanges)
        {
            NetworkInterfaceController _nic;
            _nic.RegisterInterface("eth0");
            _nic.EnableInterface("eth0");
            _nic.SetMtu("eth0", 9000);
            _nic.SetIpv4Address("eth0", "10.0.0.1");
            auto _info = _nic.GetInterfaceInfo("eth0");
            ASSERT_TRUE(_info.HasValue());
            EXPECT_EQ(_info.Value().Name, "eth0");
            EXPECT_EQ(_info.Value().Status, LinkStatus::kUp);
            EXPECT_EQ(_info.Value().Mtu, 9000U);
            EXPECT_EQ(_info.Value().Ipv4Address, "10.0.0.1");
        }
    }
}
