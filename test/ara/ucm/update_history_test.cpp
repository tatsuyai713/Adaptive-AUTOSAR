#include <gtest/gtest.h>
#include "../../../src/ara/ucm/update_history.h"

namespace ara
{
    namespace ucm
    {
        TEST(UpdateHistoryTest, RecordAndGetHistory)
        {
            UpdateHistory _hist;
            UpdateHistoryEntry _entry;
            _entry.SessionId = "s1";
            _entry.PackageName = "pkg1";
            _entry.TargetCluster = "cluster1";
            _entry.FromVersion = "1.0.0";
            _entry.ToVersion = "2.0.0";
            _entry.TimestampEpochMs = 1000U;
            _entry.Success = true;
            _entry.ErrorDescription = "";

            auto _result = _hist.RecordUpdate(_entry);
            EXPECT_TRUE(_result.HasValue());

            auto _all = _hist.GetHistory();
            EXPECT_EQ(_all.size(), 1U);
            EXPECT_EQ(_all[0].PackageName, "pkg1");
        }

        TEST(UpdateHistoryTest, RecordWithEmptyFieldsFails)
        {
            UpdateHistory _hist;
            UpdateHistoryEntry _entry;
            auto _result = _hist.RecordUpdate(_entry);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(UpdateHistoryTest, GetHistoryForCluster)
        {
            UpdateHistory _hist;

            _hist.RecordUpdate({"s1", "pkg1", "c1", "1.0", "2.0", 100U, true, ""});
            _hist.RecordUpdate({"s2", "pkg2", "c2", "1.0", "2.0", 200U, true, ""});
            _hist.RecordUpdate({"s3", "pkg3", "c1", "2.0", "3.0", 300U, false, "err"});

            auto _c1 = _hist.GetHistoryForCluster("c1");
            EXPECT_EQ(_c1.size(), 2U);

            auto _c2 = _hist.GetHistoryForCluster("c2");
            EXPECT_EQ(_c2.size(), 1U);
        }

        TEST(UpdateHistoryTest, Clear)
        {
            UpdateHistory _hist;
            _hist.RecordUpdate({"s1", "pkg1", "c1", "1.0", "2.0", 100U, true, ""});
            _hist.Clear();
            EXPECT_EQ(_hist.GetHistory().size(), 0U);
        }

        TEST(UpdateHistoryTest, SaveAndLoadRoundTrip)
        {
            const std::string _path{"/tmp/autosar_test_update_history.csv"};

            UpdateHistory _hist;
            _hist.RecordUpdate({"s1", "pkg1", "c1", "1.0", "2.0", 100U, true, ""});
            _hist.RecordUpdate({"s2", "pkg2", "c2", "2.0", "3.0", 200U, false, "checksum mismatch"});

            auto _saveResult = _hist.SaveToFile(_path);
            EXPECT_TRUE(_saveResult.HasValue());

            UpdateHistory _hist2;
            auto _loadResult = _hist2.LoadFromFile(_path);
            EXPECT_TRUE(_loadResult.HasValue());

            auto _all = _hist2.GetHistory();
            EXPECT_EQ(_all.size(), 2U);
            EXPECT_EQ(_all[0].SessionId, "s1");
            EXPECT_TRUE(_all[0].Success);
            EXPECT_EQ(_all[1].SessionId, "s2");
            EXPECT_FALSE(_all[1].Success);
        }
    }
}
