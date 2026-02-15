#include <gtest/gtest.h>
#include "../../../src/ara/diag/dtc_information.h"
#include "../../../src/ara/diag/diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        class DtcInformationTest : public testing::Test
        {
        protected:
            const ControlDtcStatusType cInitialControlDtcStatus{
                ControlDtcStatusType::kDTCSettingOff};

            ara::core::InstanceSpecifier Specifier;
            ControlDtcStatusType NotifiedControlDtcStatus;
            uint8_t LastChangedDtc;
            UdsDtcStatusByteType LastChangedDtcOldStatusByte;
            UdsDtcStatusByteType LastChangedDtcNewStatusByte;
            uint32_t NotifiedNumberOfStoredEntries;

        public:
            DtcInformationTest() : Specifier{"Instance0"},
                                   NotifiedControlDtcStatus{cInitialControlDtcStatus},
                                   LastChangedDtc{0},
                                   NotifiedNumberOfStoredEntries{0}
            {
                LastChangedDtcOldStatusByte.encodedBits = 0;
                LastChangedDtcNewStatusByte.encodedBits = 0;
            }

            void OnControlDtcStatusChanged(ControlDtcStatusType controlDtcStatus)
            {
                NotifiedControlDtcStatus = controlDtcStatus;
            }

            void OnDtcStatusChanged(
                uint32_t dtc,
                UdsDtcStatusByteType oldStatusByte,
                UdsDtcStatusByteType newStatusByte)
            {
                LastChangedDtc = dtc;
                LastChangedDtcOldStatusByte = oldStatusByte;
                LastChangedDtcNewStatusByte = newStatusByte;
            }

            void OnNumberOfStoredEntriesChanged(uint32_t numberOfStoredEntries)
            {
                NotifiedNumberOfStoredEntries = numberOfStoredEntries;
            };
        };

        TEST_F(DtcInformationTest, Constructor)
        {
            uint32_t cExpectedResult{0};

            DTCInformation _dtcInformation(Specifier);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, CurrentStatusProperty)
        {
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _expectedResult;
            _expectedResult.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _expectedResult);
            auto _actualResult{_dtcInformation.GetCurrentStatus(cDtc)};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(_expectedResult.encodedBits, _actualResult.Value().encodedBits);
        };

        TEST_F(DtcInformationTest, DtcStatusChangedNotifier)
        {
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cOldStatus{0x00};
            const uint8_t cNewStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cOldStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            _dtcInformation.SetDTCStatusChangedNotifier(
                std::bind(
                    &DtcInformationTest::OnDtcStatusChanged,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2,
                    std::placeholders::_3));

            _statusByte.encodedBits = cNewStatus;
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);

            EXPECT_EQ(cDtc, LastChangedDtc);
            EXPECT_EQ(cOldStatus, LastChangedDtcOldStatusByte.encodedBits);
            EXPECT_EQ(cNewStatus, LastChangedDtcNewStatusByte.encodedBits);
        };

        TEST_F(DtcInformationTest, NumberOfStoredEntriesProperty)
        {
            const uint32_t cExpectedResult{1};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, NumberOfStoredEntriesNotifier)
        {
            const uint32_t cExpectedResult{1};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetNumberOfStoredEntriesNotifier(
                std::bind(
                    &DtcInformationTest::OnNumberOfStoredEntriesChanged,
                    this, std::placeholders::_1));

            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);

            EXPECT_EQ(cExpectedResult, NotifiedNumberOfStoredEntries);
        };

        TEST_F(DtcInformationTest, StoredDtcIdsProperty)
        {
            const uint32_t cDtcA{0x100};
            const uint32_t cDtcB{0x200};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            UdsDtcStatusByteType _statusByte{0x01};

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtcA, cMask, _statusByte);
            _dtcInformation.SetCurrentStatus(cDtcB, cMask, _statusByte);

            auto _ids = _dtcInformation.GetStoredDtcIds();
            ASSERT_TRUE(_ids.HasValue());
            EXPECT_EQ(2U, _ids.Value().size());
            EXPECT_EQ(cDtcA, _ids.Value().at(0));
            EXPECT_EQ(cDtcB, _ids.Value().at(1));
        };

        TEST_F(DtcInformationTest, ClearMethod)
        {
            const uint32_t cExpectedResult{0};
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            const uint8_t cStatus{0x01};

            UdsDtcStatusByteType _statusByte;
            _statusByte.encodedBits = cStatus;

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);
            _dtcInformation.Clear(cDtc);
            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, ClearUnknownDtcReturnsError)
        {
            DTCInformation _dtcInformation(Specifier);
            auto _result = _dtcInformation.Clear(0xBEEF);
            EXPECT_FALSE(_result.HasValue());
            EXPECT_EQ(
                DiagErrc::kWrongDtc,
                static_cast<DiagErrc>(_result.Error().Value()));
        };

        TEST_F(DtcInformationTest, ClearAllMethod)
        {
            const uint32_t cDtc{1};
            const UdsDtcStatusBitType cMask{UdsDtcStatusBitType::kTestFailed};
            UdsDtcStatusByteType _statusByte{0x01};

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.SetCurrentStatus(cDtc, cMask, _statusByte);

            auto _clearResult = _dtcInformation.ClearAll();
            EXPECT_TRUE(_clearResult.HasValue());

            auto _actualResult{_dtcInformation.GetNumberOfStoredEntries()};
            ASSERT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(0U, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, ControlDtcStatusProperty)
        {
            ControlDtcStatusType cExpectedResult{ControlDtcStatusType::kDTCSettingOn};

            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.EnableControlDtc();
            auto _actualResult{_dtcInformation.GetControlDTCStatus()};

            EXPECT_TRUE(_actualResult.HasValue());
            EXPECT_EQ(cExpectedResult, _actualResult.Value());
        };

        TEST_F(DtcInformationTest, ControlDtcStatusNotifier)
        {
            ControlDtcStatusType cExpectedResult{ControlDtcStatusType::kDTCSettingOn};

            DTCInformation _dtcInformation(Specifier);

            _dtcInformation.SetControlDtcStatusNotifier(
                std::bind(
                    &DtcInformationTest::OnControlDtcStatusChanged,
                    this, std::placeholders::_1));

            _dtcInformation.EnableControlDtc();

            EXPECT_EQ(cExpectedResult, NotifiedControlDtcStatus);
        };

        TEST_F(DtcInformationTest, DisableControlDtcProperty)
        {
            DTCInformation _dtcInformation(Specifier);
            _dtcInformation.EnableControlDtc();
            auto _disableResult = _dtcInformation.DisableControlDtc();
            EXPECT_TRUE(_disableResult.HasValue());

            auto _statusResult = _dtcInformation.GetControlDTCStatus();
            ASSERT_TRUE(_statusResult.HasValue());
            EXPECT_EQ(ControlDtcStatusType::kDTCSettingOff, _statusResult.Value());
        };

        TEST_F(DtcInformationTest, EmptyNotifierShouldBeRejected)
        {
            DTCInformation _dtcInformation(Specifier);

            auto _dtcStatusNotifierResult =
                _dtcInformation.SetDTCStatusChangedNotifier({});
            EXPECT_FALSE(_dtcStatusNotifierResult.HasValue());
            EXPECT_EQ(
                DiagErrc::kInvalidArgument,
                static_cast<DiagErrc>(_dtcStatusNotifierResult.Error().Value()));

            auto _entryNotifierResult =
                _dtcInformation.SetNumberOfStoredEntriesNotifier({});
            EXPECT_FALSE(_entryNotifierResult.HasValue());
            EXPECT_EQ(
                DiagErrc::kInvalidArgument,
                static_cast<DiagErrc>(_entryNotifierResult.Error().Value()));

            auto _controlNotifierResult =
                _dtcInformation.SetControlDtcStatusNotifier({});
            EXPECT_FALSE(_controlNotifierResult.HasValue());
            EXPECT_EQ(
                DiagErrc::kInvalidArgument,
                static_cast<DiagErrc>(_controlNotifierResult.Error().Value()));
        };
    }
}
