#include <gtest/gtest.h>
#include "../../../../src/ara/com/e2e/profile07.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            TEST(Profile07Test, InvalidProtectionEmptyPayload)
            {
                Profile07 _profile;
                const std::vector<uint8_t> cEmpty;
                std::vector<uint8_t> _protected;
                EXPECT_FALSE(_profile.TryProtect(cEmpty, _protected));
            }

            TEST(Profile07Test, ValidProtectionAddsHeader)
            {
                Profile07 _profile;
                const std::vector<uint8_t> cPayload{0x01, 0x02, 0x03, 0x04};
                std::vector<uint8_t> _protected;
                ASSERT_TRUE(_profile.TryProtect(cPayload, _protected));
                // Header is 16 bytes + 4 bytes payload
                EXPECT_EQ(_protected.size(), 20U);
            }

            TEST(Profile07Test, ProtectedDataPassesCheck)
            {
                Profile07 _sender;
                Profile07 _receiver;
                const std::vector<uint8_t> cPayload{0xDE, 0xAD, 0xBE, 0xEF};
                std::vector<uint8_t> _protected;
                ASSERT_TRUE(_sender.TryProtect(cPayload, _protected));
                EXPECT_EQ(_receiver.Check(_protected), CheckStatusType::kOk);
            }

            TEST(Profile07Test, BitFlipDetectedAsWrongCrc)
            {
                Profile07 _sender;
                Profile07 _receiver;
                const std::vector<uint8_t> cPayload{0x11, 0x22, 0x33, 0x44};
                std::vector<uint8_t> _protected;
                ASSERT_TRUE(_sender.TryProtect(cPayload, _protected));
                // Flip a payload byte (after 16-byte header)
                _protected[16] ^= 0xFF;
                EXPECT_EQ(_receiver.Check(_protected), CheckStatusType::kWrongCrc);
            }

            TEST(Profile07Test, RepeatedMessageDetected)
            {
                Profile07 _sender;
                Profile07 _receiver;
                const std::vector<uint8_t> cPayload{0xAA, 0xBB};
                std::vector<uint8_t> _protected;
                ASSERT_TRUE(_sender.TryProtect(cPayload, _protected));
                EXPECT_EQ(_receiver.Check(_protected), CheckStatusType::kOk);
                // Check same message again → repeated
                EXPECT_EQ(_receiver.Check(_protected), CheckStatusType::kRepeated);
            }

            TEST(Profile07Test, TooShortMessageReturnsNoNewData)
            {
                Profile07 _profile;
                // Fewer than 17 bytes (header=16 + at least 1 payload byte)
                const std::vector<uint8_t> cShort(10, 0x00);
                EXPECT_EQ(_profile.Check(cShort), CheckStatusType::kNoNewData);
            }

            TEST(Profile07Test, InvalidForwardEmptyPayload)
            {
                Profile07 _profile;
                const std::vector<uint8_t> cEmpty;
                std::vector<uint8_t> _protected;
                EXPECT_FALSE(_profile.TryForward(cEmpty, _protected));
            }

            TEST(Profile07Test, ForwardReplicatesLastCheckedCounter)
            {
                Profile07Config cfg;
                cfg.dataId = 0x12345678U;
                Profile07 _sender(cfg);
                Profile07 _forwarder(cfg);
                Profile07 _receiver(cfg);

                const std::vector<uint8_t> cPayload{0x01, 0x02, 0x03, 0x04};

                // Let forwarder check one message to capture counter (counter 1, delta 1 from 0)
                std::vector<uint8_t> _protectedBySender;
                ASSERT_TRUE(_sender.TryProtect(cPayload, _protectedBySender));
                ASSERT_EQ(_forwarder.Check(_protectedBySender), CheckStatusType::kOk);

                // Forwarder re-encodes using last checked counter; receiver should accept
                std::vector<uint8_t> _forwarded;
                ASSERT_TRUE(_forwarder.TryForward(cPayload, _forwarded));
                EXPECT_EQ(_receiver.Check(_forwarded), CheckStatusType::kOk);
            }

            TEST(Profile07Test, CounterWrapsAndContinues)
            {
                Profile07Config cfg;
                cfg.maxDeltaCounter = 1;
                Profile07 _sender(cfg);
                Profile07 _receiver(cfg);

                const std::vector<uint8_t> cPayload{0xFF};
                std::vector<uint8_t> _protected;

                // Send 5 messages; all should be accepted in sequence
                for (std::size_t i = 0; i < 5; ++i)
                {
                    ASSERT_TRUE(_sender.TryProtect(cPayload, _protected));
                    EXPECT_EQ(_receiver.Check(_protected), CheckStatusType::kOk)
                        << "Failed at message " << i;
                }
            }

            TEST(Profile07Test, CustomDataIdAffectsCrc)
            {
                Profile07Config cfg1;
                cfg1.dataId = 0x00000001U;
                Profile07Config cfg2;
                cfg2.dataId = 0x00000002U;

                Profile07 _sender1(cfg1);
                Profile07 _receiver2(cfg2);

                const std::vector<uint8_t> cPayload{0x55, 0x55};
                std::vector<uint8_t> _protected;
                ASSERT_TRUE(_sender1.TryProtect(cPayload, _protected));
                // Different DataID → CRC mismatch
                EXPECT_EQ(_receiver2.Check(_protected), CheckStatusType::kWrongCrc);
            }
        }
    }
}
