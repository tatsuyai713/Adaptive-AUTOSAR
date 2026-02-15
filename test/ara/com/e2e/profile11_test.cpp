#include <gtest/gtest.h>
#include "../../../../src/ara/com/e2e/profile11.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            TEST(Profile11Test, InvalidProtection)
            {
                Profile11 _profile;

                const std::vector<uint8_t> cUnprotectedData;
                std::vector<uint8_t> _protectedData;

                const bool cResult{_profile.TryProtect(cUnprotectedData, _protectedData)};
                EXPECT_FALSE(cResult);
            }

            TEST(Profile11Test, ValidProtection)
            {
                Profile11 _profile;

                const size_t cCrcOffset{0};
                const uint8_t cExpectedResult{0x9f};

                const std::vector<uint8_t> cUnprotectedData{0x12, 0x34, 0x56, 0x78};
                std::vector<uint8_t> _protectedData;

                const bool cResult{_profile.TryProtect(cUnprotectedData, _protectedData)};
                EXPECT_TRUE(cResult);

                const uint8_t cActualResult{_protectedData.at(cCrcOffset)};
                EXPECT_EQ(cActualResult, cExpectedResult);
            }

            TEST(Profile11Test, InvalidForward)
            {
                Profile11 _profile;

                const std::vector<uint8_t> cUnprotectedData;
                std::vector<uint8_t> _protectedData;

                const bool cResult{_profile.TryForward(cUnprotectedData, _protectedData)};
                EXPECT_FALSE(cResult);
            }

            TEST(Profile11Test, ForwardReplicatesLastCheckedCounter)
            {
                Profile11 _senderProfile;
                Profile11 _forwardProfile;

                const std::vector<uint8_t> cPayload{0x12, 0x34, 0x56, 0x78};

                std::vector<uint8_t> _receivedProtectedData;
                for (std::size_t i = 0; i < 7; ++i)
                {
                    ASSERT_TRUE(
                        _senderProfile.TryProtect(cPayload, _receivedProtectedData));
                }

                // Consume a protected frame so forwarding reuses the observed counter.
                EXPECT_EQ(
                    CheckStatusType::kOk,
                    _forwardProfile.Check(_receivedProtectedData));

                std::vector<uint8_t> _forwardedProtectedData;
                ASSERT_TRUE(
                    _forwardProfile.TryForward(cPayload, _forwardedProtectedData));
                ASSERT_EQ(cPayload.size() + 2U, _forwardedProtectedData.size());

                const uint8_t cReceivedCounter{
                    static_cast<uint8_t>(_receivedProtectedData[1] & 0x0f)};
                const uint8_t cForwardedCounter{
                    static_cast<uint8_t>(_forwardedProtectedData[1] & 0x0f)};

                EXPECT_EQ(cReceivedCounter, cForwardedCounter);
                EXPECT_EQ(
                    0xf0,
                    static_cast<uint8_t>(_forwardedProtectedData[1] & 0xf0));

                // Generated frame must still pass E2E validation.
                Profile11 _receiverProfile;
                EXPECT_EQ(
                    CheckStatusType::kOk,
                    _receiverProfile.Check(_forwardedProtectedData));
            }

            TEST(Profile11Test, ProtectAfterForwardContinuesCounter)
            {
                Profile11 _senderProfile;
                Profile11 _forwardProfile;

                const std::vector<uint8_t> cPayload{0x10, 0x20, 0x30, 0x40};

                std::vector<uint8_t> _receivedProtectedData;
                for (std::size_t i = 0; i < 4; ++i)
                {
                    ASSERT_TRUE(
                        _senderProfile.TryProtect(cPayload, _receivedProtectedData));
                }

                ASSERT_EQ(CheckStatusType::kOk, _forwardProfile.Check(_receivedProtectedData));
                std::vector<uint8_t> _forwardedProtectedData;
                ASSERT_TRUE(
                    _forwardProfile.TryForward(cPayload, _forwardedProtectedData));

                std::vector<uint8_t> _nextProtectedData;
                ASSERT_TRUE(
                    _forwardProfile.TryProtect(cPayload, _nextProtectedData));

                const uint8_t cForwardedCounter{
                    static_cast<uint8_t>(_forwardedProtectedData[1] & 0x0f)};
                const uint8_t cNextCounter{
                    static_cast<uint8_t>(_nextProtectedData[1] & 0x0f)};

                EXPECT_EQ(
                    static_cast<uint8_t>((cForwardedCounter + 1U) & 0x0f),
                    cNextCounter);
            }

            TEST(Profile11Test, NoNewDataCheck)
            {
                Profile11 _profile;

                const CheckStatusType cExpectedResult{CheckStatusType::kNoNewData};
                const std::vector<uint8_t> cProtectedData;

                const CheckStatusType cActualResult{_profile.Check(cProtectedData)};
                EXPECT_EQ(cActualResult, cExpectedResult);
            }

            TEST(Profile11Test, WrongCrcCheck)
            {
                Profile11 _profile;

                const CheckStatusType cExpectedResult{CheckStatusType::kWrongCrc};
                const std::vector<uint8_t> cProtectedData{0x00, 0xf1, 0x12, 0x34, 0x56, 0x78};

                const CheckStatusType cActualResult{_profile.Check(cProtectedData)};
                EXPECT_EQ(cActualResult, cExpectedResult);
            }

            TEST(Profile11Test, RepeatedCheck)
            {
                Profile11 _profile;

                const CheckStatusType cExpectedResult{CheckStatusType::kRepeated};
                const std::vector<uint8_t> cProtectedData{0xf5, 0xf0, 0x12, 0x34, 0x56, 0x78};

                const CheckStatusType cActualResult{_profile.Check(cProtectedData)};
                EXPECT_EQ(cActualResult, cExpectedResult);
            }

            TEST(Profile11Test, WrongSequenceCheckScenario)
            {
                Profile11 _profile;

                const CheckStatusType cFirstExpectedResult{CheckStatusType::kOk};
                const std::vector<uint8_t> cFirstProtectedData{0xf5, 0xf0, 0x12, 0x34, 0x56, 0x78};

                const CheckStatusType cLastExpectedResult{CheckStatusType::kWrongSequence};
                const std::vector<uint8_t> cLastProtectedData{0x9f, 0xf1, 0x12, 0x34, 0x56, 0x78};

                const CheckStatusType cFirstActualResult{_profile.Check(cLastProtectedData)};
                EXPECT_EQ(cFirstActualResult, cFirstExpectedResult);

                const CheckStatusType cLastActualResult{_profile.Check(cFirstProtectedData)};
                EXPECT_EQ(cLastActualResult, cLastExpectedResult);
            }
        }
    }
}
