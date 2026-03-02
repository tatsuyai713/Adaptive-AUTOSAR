#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "../../../src/ara/nm/nm_pdu.h"

namespace ara
{
    namespace nm
    {
        TEST(NmPduTest, DefaultConstruction)
        {
            NmPdu _pdu;
            EXPECT_EQ(_pdu.SourceNodeId, 0U);
            EXPECT_EQ(_pdu.ControlBitVector, 0U);
            EXPECT_TRUE(_pdu.UserData.empty());
            EXPECT_TRUE(_pdu.PnFilterMask.empty());
        }

        TEST(NmPduTest, SetAndClearControlBit)
        {
            NmPdu _pdu;

            _pdu.SetControlBit(NmControlBit::kRepeatMessageRequest);
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kRepeatMessageRequest));
            EXPECT_FALSE(_pdu.HasControlBit(NmControlBit::kActiveWakeup));

            _pdu.SetControlBit(NmControlBit::kActiveWakeup);
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kActiveWakeup));

            _pdu.ClearControlBit(NmControlBit::kRepeatMessageRequest);
            EXPECT_FALSE(_pdu.HasControlBit(NmControlBit::kRepeatMessageRequest));
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kActiveWakeup));
        }

        TEST(NmPduTest, SerializeMinimal)
        {
            NmPdu _pdu;
            _pdu.SourceNodeId = 0x42;
            _pdu.ControlBitVector = 0x01;

            auto _result = _pdu.Serialize();
            ASSERT_TRUE(_result.HasValue());

            auto &buf = _result.Value();
            EXPECT_EQ(buf.size(), 2U);
            EXPECT_EQ(buf[0], 0x42);
            EXPECT_EQ(buf[1], 0x01);
        }

        TEST(NmPduTest, SerializeWithUserData)
        {
            NmPdu _pdu;
            _pdu.SourceNodeId = 0x10;
            _pdu.ControlBitVector = 0x00;
            _pdu.UserData = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

            auto _result = _pdu.Serialize();
            ASSERT_TRUE(_result.HasValue());

            auto &buf = _result.Value();
            EXPECT_EQ(buf.size(), 8U);
            EXPECT_EQ(buf[0], 0x10);
            EXPECT_EQ(buf[2], 0xAA);
            EXPECT_EQ(buf[7], 0xFF);
        }

        TEST(NmPduTest, SerializeWithPnFilterMask)
        {
            NmPdu _pdu;
            _pdu.SourceNodeId = 0x01;
            _pdu.SetControlBit(NmControlBit::kPartialNetwork);
            _pdu.UserData = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            _pdu.PnFilterMask = {0x01, 0x02, 0x03};

            auto _result = _pdu.Serialize();
            ASSERT_TRUE(_result.HasValue());

            auto &buf = _result.Value();
            EXPECT_EQ(buf.size(), 11U); // 2 header + 6 data + 3 PN
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kPartialNetwork));
        }

        TEST(NmPduTest, DeserializeRoundTrip)
        {
            NmPdu _original;
            _original.SourceNodeId = 0x55;
            _original.SetControlBit(NmControlBit::kActiveWakeup);
            _original.SetControlBit(NmControlBit::kPnEnabled);
            _original.UserData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

            auto _serResult = _original.Serialize();
            ASSERT_TRUE(_serResult.HasValue());

            auto _deResult = NmPdu::Deserialize(_serResult.Value());
            ASSERT_TRUE(_deResult.HasValue());

            auto &_parsed = _deResult.Value();
            EXPECT_EQ(_parsed.SourceNodeId, 0x55);
            EXPECT_TRUE(_parsed.HasControlBit(NmControlBit::kActiveWakeup));
            EXPECT_TRUE(_parsed.HasControlBit(NmControlBit::kPnEnabled));
            EXPECT_EQ(_parsed.UserData.size(), 6U);
            EXPECT_EQ(_parsed.UserData[0], 0x01);
        }

        TEST(NmPduTest, DeserializeTooShortFails)
        {
            std::vector<std::uint8_t> shortData{0x01};
            auto _result = NmPdu::Deserialize(shortData);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(NmPduTest, DeserializeWithPnMask)
        {
            // Build a PDU with PN mask manually:
            // header(2) + user_data(6) + pn_mask(3)
            std::vector<std::uint8_t> raw{
                0x20, 0x08, // nodeId=0x20, CBV=kPartialNetwork
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // user data
                0xF0, 0x0F, 0xAA  // PN filter mask
            };

            auto _result = NmPdu::Deserialize(raw, 6U);
            ASSERT_TRUE(_result.HasValue());

            auto &pdu = _result.Value();
            EXPECT_EQ(pdu.SourceNodeId, 0x20);
            EXPECT_TRUE(pdu.HasControlBit(NmControlBit::kPartialNetwork));
            EXPECT_EQ(pdu.UserData.size(), 6U);
            EXPECT_EQ(pdu.PnFilterMask.size(), 3U);
            EXPECT_EQ(pdu.PnFilterMask[0], 0xF0);
            EXPECT_EQ(pdu.PnFilterMask[2], 0xAA);
        }

        TEST(NmPduTest, MultipleControlBits)
        {
            NmPdu _pdu;
            _pdu.SetControlBit(NmControlBit::kRepeatMessageRequest);
            _pdu.SetControlBit(NmControlBit::kNmCoordinatorSleep);
            _pdu.SetControlBit(NmControlBit::kPartialNetwork);
            _pdu.SetControlBit(NmControlBit::kPnEnabled);

            EXPECT_EQ(_pdu.ControlBitVector, 0x1BU);
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kRepeatMessageRequest));
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kNmCoordinatorSleep));
            EXPECT_FALSE(_pdu.HasControlBit(NmControlBit::kActiveWakeup));
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kPartialNetwork));
            EXPECT_TRUE(_pdu.HasControlBit(NmControlBit::kPnEnabled));
        }
    }
}
