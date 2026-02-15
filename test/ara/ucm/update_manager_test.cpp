#include <gtest/gtest.h>

#include <vector>

#include "../../../src/ara/ucm/update_manager.h"

namespace ara
{
    namespace ucm
    {
        namespace
        {
            std::vector<std::uint8_t> GetAbcSha256Digest()
            {
                return {
                    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                    0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                    0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                    0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
            }
        }

        TEST(UpdateManagerTest, HappyPathPrepareStageVerifyActivate)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.2.3"};
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            ASSERT_TRUE(_manager.PrepareUpdate("session-1").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metadata,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());

            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kActivated);
            EXPECT_EQ(_manager.GetActiveVersion(), "1.2.3");
        }

        TEST(UpdateManagerTest, VerifyFailsForDigestMismatch)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.2.3"};
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};
            const std::vector<std::uint8_t> cWrongDigest(32U, 0x00U);

            ASSERT_TRUE(_manager.PrepareUpdate("session-2").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metadata,
                            cPayload,
                            cWrongDigest)
                    .HasValue());

            auto _verifyResult = _manager.VerifyStagedSoftwarePackage();
            ASSERT_FALSE(_verifyResult.HasValue());
            EXPECT_STREQ(_verifyResult.Error().Domain().Name(), "Ucm");
        }

        TEST(UpdateManagerTest, ActivateWithoutVerificationReturnsInvalidState)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "2.0.0"};
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            ASSERT_TRUE(_manager.PrepareUpdate("session-3").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metadata,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());

            auto _activateResult = _manager.ActivateSoftwarePackage();
            ASSERT_FALSE(_activateResult.HasValue());
            EXPECT_STREQ(_activateResult.Error().Domain().Name(), "Ucm");
        }

        TEST(UpdateManagerTest, RollbackAfterActivationRestoresPreviousVersion)
        {
            UpdateManager _manager;
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            SoftwarePackageMetadata _versionOne{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.0.0"};
            ASSERT_TRUE(_manager.PrepareUpdate("session-v1").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _versionOne,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());
            ASSERT_EQ(_manager.GetActiveVersion(), "1.0.0");

            SoftwarePackageMetadata _versionTwo{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "2.0.0"};
            ASSERT_TRUE(_manager.PrepareUpdate("session-v2").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _versionTwo,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());
            ASSERT_EQ(_manager.GetActiveVersion(), "2.0.0");

            ASSERT_TRUE(_manager.RollbackSoftwarePackage().HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kRolledBack);
            EXPECT_EQ(_manager.GetActiveVersion(), "1.0.0");
        }

        TEST(UpdateManagerTest, StateAndProgressHandlersAreInvoked)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "3.0.0"};
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            std::vector<UpdateSessionState> _states;
            std::vector<std::uint8_t> _progresses;

            ASSERT_TRUE(_manager.SetStateChangeHandler(
                                    [&_states](UpdateSessionState state)
                                    {
                                        _states.push_back(state);
                                    })
                            .HasValue());
            ASSERT_TRUE(_manager.SetProgressHandler(
                                    [&_progresses](std::uint8_t progress)
                                    {
                                        _progresses.push_back(progress);
                                    })
                            .HasValue());

            ASSERT_TRUE(_manager.PrepareUpdate("session-handler").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metadata,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());

            ASSERT_FALSE(_states.empty());
            ASSERT_FALSE(_progresses.empty());
            EXPECT_EQ(_states.back(), UpdateSessionState::kActivated);
            EXPECT_EQ(_progresses.back(), 100U);
        }

        TEST(UpdateManagerTest, CancelUpdateSessionClearsSessionAndStage)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.2.3"};
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            ASSERT_TRUE(_manager.PrepareUpdate("session-cancel").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metadata,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());

            ASSERT_TRUE(_manager.CancelUpdateSession().HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kCancelled);
            EXPECT_TRUE(_manager.GetSessionId().empty());

            auto _staged = _manager.GetStagedSoftwarePackageMetadata();
            EXPECT_FALSE(_staged.HasValue());
        }

        TEST(UpdateManagerTest, ClusterVersionLookupAndKnownClusterList)
        {
            UpdateManager _manager;
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            SoftwarePackageMetadata _metaClusterA{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.0.0"};
            ASSERT_TRUE(_manager.PrepareUpdate("cluster-a").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _metaClusterA,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());

            auto _clusterResult =
                _manager.GetClusterVersion("VehicleControlCluster");
            ASSERT_TRUE(_clusterResult.HasValue());
            EXPECT_EQ(_clusterResult.Value(), "1.0.0");

            auto _clusters = _manager.GetKnownClusters();
            ASSERT_EQ(_clusters.size(), 1U);
            EXPECT_EQ(_clusters[0], "VehicleControlCluster");
        }

        TEST(UpdateManagerTest, DowngradeIsRejected)
        {
            UpdateManager _manager;
            const std::vector<std::uint8_t> cPayload{'a', 'b', 'c'};

            SoftwarePackageMetadata _versionTwo{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "2.0.0"};
            ASSERT_TRUE(_manager.PrepareUpdate("upgrade-first").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _versionTwo,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());

            SoftwarePackageMetadata _versionOne{
                "VehicleControlAppPkg",
                "VehicleControlCluster",
                "1.0.0"};
            ASSERT_TRUE(_manager.PrepareUpdate("downgrade-second").HasValue());
            ASSERT_TRUE(
                _manager.StageSoftwarePackage(
                            _versionOne,
                            cPayload,
                            GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());

            auto _activateResult = _manager.ActivateSoftwarePackage();
            ASSERT_FALSE(_activateResult.HasValue());
            EXPECT_STREQ(_activateResult.Error().Domain().Name(), "Ucm");
        }

        // ---- Transfer API tests ----

        TEST(UpdateManagerTest, TransferHappyPath)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg",
                "VehicleControlCluster",
                "1.0.0"};
            const std::vector<std::uint8_t> cChunk1{'a'};
            const std::vector<std::uint8_t> cChunk2{'b', 'c'};

            ASSERT_TRUE(_manager.PrepareUpdate("transfer-1").HasValue());
            ASSERT_TRUE(
                _manager.TransferStart(_metadata, 3U, GetAbcSha256Digest())
                    .HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kTransferring);

            ASSERT_TRUE(_manager.TransferData(cChunk1).HasValue());
            ASSERT_TRUE(_manager.TransferData(cChunk2).HasValue());
            ASSERT_TRUE(_manager.TransferExit().HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kPackageStaged);

            ASSERT_TRUE(_manager.VerifyStagedSoftwarePackage().HasValue());
            ASSERT_TRUE(_manager.ActivateSoftwarePackage().HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kActivated);
            EXPECT_EQ(_manager.GetActiveVersion(), "1.0.0");
        }

        TEST(UpdateManagerTest, TransferStartFailsWhenNotPrepared)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};

            auto _result = _manager.TransferStart(
                _metadata, 10U, GetAbcSha256Digest());
            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Ucm");
        }

        TEST(UpdateManagerTest, TransferDataFailsWhenNotTransferring)
        {
            UpdateManager _manager;
            ASSERT_TRUE(_manager.PrepareUpdate("s1").HasValue());

            auto _result = _manager.TransferData({'x'});
            ASSERT_FALSE(_result.HasValue());
            EXPECT_STREQ(_result.Error().Domain().Name(), "Ucm");
        }

        TEST(UpdateManagerTest, TransferExitSizeMismatch)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};

            ASSERT_TRUE(_manager.PrepareUpdate("s2").HasValue());
            ASSERT_TRUE(
                _manager.TransferStart(_metadata, 100U, GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.TransferData({'a', 'b'}).HasValue());

            auto _result = _manager.TransferExit();
            ASSERT_FALSE(_result.HasValue());
        }

        TEST(UpdateManagerTest, TransferExitEmptyBuffer)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};

            ASSERT_TRUE(_manager.PrepareUpdate("s3").HasValue());
            ASSERT_TRUE(
                _manager.TransferStart(_metadata, 5U, GetAbcSha256Digest())
                    .HasValue());

            auto _result = _manager.TransferExit();
            ASSERT_FALSE(_result.HasValue());
        }

        TEST(UpdateManagerTest, TransferStartInvalidMetadata)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _badMeta{"", "Cluster", "1.0.0"};

            ASSERT_TRUE(_manager.PrepareUpdate("s4").HasValue());
            auto _result = _manager.TransferStart(
                _badMeta, 10U, GetAbcSha256Digest());
            ASSERT_FALSE(_result.HasValue());
        }

        TEST(UpdateManagerTest, TransferStartInvalidDigestLength)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};
            const std::vector<std::uint8_t> cBadDigest(16U, 0x00U);

            ASSERT_TRUE(_manager.PrepareUpdate("s5").HasValue());
            auto _result = _manager.TransferStart(
                _metadata, 10U, cBadDigest);
            ASSERT_FALSE(_result.HasValue());
        }

        TEST(UpdateManagerTest, CancelDuringTransfer)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};

            ASSERT_TRUE(_manager.PrepareUpdate("s6").HasValue());
            ASSERT_TRUE(
                _manager.TransferStart(_metadata, 10U, GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.TransferData({'a'}).HasValue());

            ASSERT_TRUE(_manager.CancelUpdateSession().HasValue());
            EXPECT_EQ(_manager.GetState(), UpdateSessionState::kCancelled);
        }

        TEST(UpdateManagerTest, TransferProgressCallbackInvoked)
        {
            UpdateManager _manager;
            SoftwarePackageMetadata _metadata{
                "OtaPkg", "Cluster", "1.0.0"};

            std::vector<UpdateSessionState> _states;
            std::vector<std::uint8_t> _progresses;

            ASSERT_TRUE(_manager.SetStateChangeHandler(
                                    [&_states](UpdateSessionState state)
                                    {
                                        _states.push_back(state);
                                    })
                            .HasValue());
            ASSERT_TRUE(_manager.SetProgressHandler(
                                    [&_progresses](std::uint8_t progress)
                                    {
                                        _progresses.push_back(progress);
                                    })
                            .HasValue());

            ASSERT_TRUE(_manager.PrepareUpdate("s7").HasValue());
            ASSERT_TRUE(
                _manager.TransferStart(_metadata, 3U, GetAbcSha256Digest())
                    .HasValue());
            ASSERT_TRUE(_manager.TransferData({'a', 'b', 'c'}).HasValue());
            ASSERT_TRUE(_manager.TransferExit().HasValue());

            ASSERT_FALSE(_states.empty());
            ASSERT_FALSE(_progresses.empty());

            // Should have seen kTransferring state
            bool _sawTransferring = false;
            for (auto s : _states)
            {
                if (s == UpdateSessionState::kTransferring)
                {
                    _sawTransferring = true;
                    break;
                }
            }
            EXPECT_TRUE(_sawTransferring);
        }
    }
}
