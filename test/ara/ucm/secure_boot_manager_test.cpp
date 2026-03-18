#include <gtest/gtest.h>
#include "../../../src/ara/ucm/secure_boot_manager.h"

namespace ara
{
    namespace ucm
    {
        TEST(SecureBootManagerTest, VerifyBootChainWithoutRootFails)
        {
            SecureBootManager _mgr;
            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kChainIncomplete);
        }

        TEST(SecureBootManagerTest, SetRootAndVerifyBootChain)
        {
            SecureBootManager _mgr;

            CertificateEntry _root;
            _root.SubjectName = "RootCA";
            _root.IssuerName = "RootCA";
            _root.PublicKey = {0x01, 0x02};
            _root.Signature = {0x03, 0x04};
            _root.NotBefore = std::chrono::system_clock::now() -
                              std::chrono::hours(24);
            _root.NotAfter = std::chrono::system_clock::now() +
                             std::chrono::hours(24 * 365);
            _mgr.SetRootCertificate(_root);

            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kVerified);
        }

        TEST(SecureBootManagerTest, AddIntermediateCertificate)
        {
            SecureBootManager _mgr;

            CertificateEntry _root;
            _root.SubjectName = "RootCA";
            _root.IssuerName = "RootCA";
            _root.PublicKey = {0x01};
            _root.Signature = {0x02};
            _root.NotBefore = std::chrono::system_clock::now() -
                              std::chrono::hours(1);
            _root.NotAfter = std::chrono::system_clock::now() +
                             std::chrono::hours(24 * 365);
            _mgr.SetRootCertificate(_root);

            CertificateEntry _inter;
            _inter.SubjectName = "IntermediateCA";
            _inter.IssuerName = "RootCA";
            _inter.PublicKey = {0x03};
            _inter.Signature = {0x04};
            _inter.NotBefore = std::chrono::system_clock::now() -
                               std::chrono::hours(1);
            _inter.NotAfter = std::chrono::system_clock::now() +
                              std::chrono::hours(24 * 365);
            _mgr.AddIntermediateCertificate(_inter);

            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kVerified);
        }

        TEST(SecureBootManagerTest, RevokeCertificate)
        {
            SecureBootManager _mgr;

            CertificateEntry _root;
            _root.SubjectName = "RootCA";
            _root.IssuerName = "RootCA";
            _root.PublicKey = {0x01};
            _root.Signature = {0x02};
            _root.NotBefore = std::chrono::system_clock::now() -
                              std::chrono::hours(1);
            _root.NotAfter = std::chrono::system_clock::now() +
                             std::chrono::hours(24 * 365);
            _mgr.SetRootCertificate(_root);

            CertificateEntry _inter;
            _inter.SubjectName = "BadCert";
            _inter.IssuerName = "RootCA";
            _inter.PublicKey = {0x03};
            _inter.Signature = {0x04};
            _inter.NotBefore = std::chrono::system_clock::now() -
                               std::chrono::hours(1);
            _inter.NotAfter = std::chrono::system_clock::now() +
                              std::chrono::hours(24 * 365);
            _mgr.AddIntermediateCertificate(_inter);

            bool _revoked = _mgr.RevokeCertificate("BadCert");
            EXPECT_TRUE(_revoked);
            EXPECT_FALSE(_mgr.HasValidCertificate("BadCert"));
        }

        TEST(SecureBootManagerTest, TpmAttestationSoftware)
        {
            SecureBootManager _mgr;
            _mgr.SetTpmQuoteType(TpmQuoteType::kSoftware);
            auto _report = _mgr.RequestTpmAttestation({0x00, 0x01, 0x02});
            EXPECT_EQ(_report.Result, BootVerifyResult::kVerified);
        }

        TEST(SecureBootManagerTest, TpmAttestationNone)
        {
            SecureBootManager _mgr;
            _mgr.SetTpmQuoteType(TpmQuoteType::kNone);
            auto _report = _mgr.RequestTpmAttestation({});
            EXPECT_EQ(_report.Result, BootVerifyResult::kTpmAttestationFailed);
        }
    }
}
