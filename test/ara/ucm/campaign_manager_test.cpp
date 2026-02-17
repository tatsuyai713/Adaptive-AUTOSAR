#include <gtest/gtest.h>
#include "../../../src/ara/ucm/campaign_manager.h"

namespace ara
{
    namespace ucm
    {
        TEST(CampaignManagerTest, CreateCampaign)
        {
            CampaignManager _mgr;
            std::vector<SoftwarePackageMetadata> _pkgs{
                {"pkg1", "cluster1", "1.0.0"},
                {"pkg2", "cluster2", "2.0.0"}};

            auto _result = _mgr.CreateCampaign("camp1", _pkgs);
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value(), "camp1");
        }

        TEST(CampaignManagerTest, CreateDuplicateFails)
        {
            CampaignManager _mgr;
            std::vector<SoftwarePackageMetadata> _pkgs{
                {"pkg1", "cluster1", "1.0.0"}};

            _mgr.CreateCampaign("camp1", _pkgs);
            auto _result = _mgr.CreateCampaign("camp1", _pkgs);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(CampaignManagerTest, CreateWithEmptyFieldsFails)
        {
            CampaignManager _mgr;
            auto _result = _mgr.CreateCampaign("", {});
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(CampaignManagerTest, StartCampaign)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("camp1",
                                {{"pkg1", "c1", "1.0.0"}});

            auto _result = _mgr.StartCampaign("camp1");
            EXPECT_TRUE(_result.HasValue());

            auto _state = _mgr.GetCampaignState("camp1");
            ASSERT_TRUE(_state.HasValue());
            EXPECT_EQ(_state.Value(), CampaignState::kInProgress);
        }

        TEST(CampaignManagerTest, StartNonexistentFails)
        {
            CampaignManager _mgr;
            auto _result = _mgr.StartCampaign("nope");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(CampaignManagerTest, AdvanceToCompleted)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("camp1",
                                {{"pkg1", "c1", "1.0.0"},
                                 {"pkg2", "c2", "2.0.0"}});
            _mgr.StartCampaign("camp1");

            _mgr.AdvancePackage("camp1", "pkg1",
                                UpdateSessionState::kActivated);

            auto _state1 = _mgr.GetCampaignState("camp1");
            EXPECT_EQ(_state1.Value(), CampaignState::kPartiallyComplete);

            _mgr.AdvancePackage("camp1", "pkg2",
                                UpdateSessionState::kActivated);

            auto _state2 = _mgr.GetCampaignState("camp1");
            EXPECT_EQ(_state2.Value(), CampaignState::kCompleted);
        }

        TEST(CampaignManagerTest, AdvanceToFailed)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("camp1",
                                {{"pkg1", "c1", "1.0.0"}});
            _mgr.StartCampaign("camp1");

            _mgr.AdvancePackage("camp1", "pkg1",
                                UpdateSessionState::kVerificationFailed);

            auto _state = _mgr.GetCampaignState("camp1");
            EXPECT_EQ(_state.Value(), CampaignState::kFailed);
        }

        TEST(CampaignManagerTest, RollbackCampaign)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("camp1",
                                {{"pkg1", "c1", "1.0.0"}});
            _mgr.StartCampaign("camp1");

            auto _result = _mgr.RollbackCampaign("camp1");
            EXPECT_TRUE(_result.HasValue());

            auto _state = _mgr.GetCampaignState("camp1");
            EXPECT_EQ(_state.Value(), CampaignState::kRolledBack);
        }

        TEST(CampaignManagerTest, GetCampaignPackages)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("camp1",
                                {{"pkg1", "c1", "1.0.0"},
                                 {"pkg2", "c2", "2.0.0"}});

            auto _result = _mgr.GetCampaignPackages("camp1");
            ASSERT_TRUE(_result.HasValue());
            EXPECT_EQ(_result.Value().size(), 2U);
        }

        TEST(CampaignManagerTest, ListCampaignIds)
        {
            CampaignManager _mgr;
            _mgr.CreateCampaign("a", {{"p1", "c1", "1.0"}});
            _mgr.CreateCampaign("b", {{"p2", "c2", "2.0"}});

            auto _ids = _mgr.ListCampaignIds();
            EXPECT_EQ(_ids.size(), 2U);
        }
    }
}
