#include <gtest/gtest.h>
#include "../../../src/ara/com/sample_info.h"

namespace ara
{
    namespace com
    {
        TEST(E2ESampleStatusTest, EnumValues)
        {
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kOk), 0U);
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kRepeated), 1U);
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kWrongSequence), 2U);
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kError), 3U);
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kNotAvailable), 4U);
            EXPECT_EQ(static_cast<uint8_t>(E2ESampleStatus::kNoCheck), 5U);
        }

        TEST(SampleInfoTest, DefaultValues)
        {
            SampleInfo info;
            EXPECT_EQ(info.SourceInstanceId, 0U);
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kNoCheck);
            EXPECT_EQ(info.SequenceNumber, 0U);
        }

        TEST(SampleInfoTest, NowFactory)
        {
            auto before = std::chrono::steady_clock::now();
            auto info = SampleInfo::Now();
            auto after = std::chrono::steady_clock::now();

            EXPECT_GE(info.ReceiveTimestamp, before);
            EXPECT_LE(info.ReceiveTimestamp, after);
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kNoCheck);
        }

        TEST(SampleInfoTest, SetFields)
        {
            SampleInfo info;
            info.SourceInstanceId = 42U;
            info.E2EStatus = E2ESampleStatus::kOk;
            info.SequenceNumber = 100U;

            EXPECT_EQ(info.SourceInstanceId, 42U);
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kOk);
            EXPECT_EQ(info.SequenceNumber, 100U);
        }

        TEST(SampleInfoTest, CopySemantics)
        {
            auto info = SampleInfo::Now();
            info.SourceInstanceId = 7U;
            info.SequenceNumber = 55U;

            SampleInfo copy = info;
            EXPECT_EQ(copy.SourceInstanceId, 7U);
            EXPECT_EQ(copy.SequenceNumber, 55U);
            EXPECT_EQ(copy.ReceiveTimestamp, info.ReceiveTimestamp);
        }

        TEST(SampleInfoTest, AllE2EStatusValues)
        {
            SampleInfo info;

            info.E2EStatus = E2ESampleStatus::kOk;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kOk);

            info.E2EStatus = E2ESampleStatus::kRepeated;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kRepeated);

            info.E2EStatus = E2ESampleStatus::kWrongSequence;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kWrongSequence);

            info.E2EStatus = E2ESampleStatus::kError;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kError);

            info.E2EStatus = E2ESampleStatus::kNotAvailable;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kNotAvailable);

            info.E2EStatus = E2ESampleStatus::kNoCheck;
            EXPECT_EQ(info.E2EStatus, E2ESampleStatus::kNoCheck);
        }
    }
}
