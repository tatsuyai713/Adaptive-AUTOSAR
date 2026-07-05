#include <gtest/gtest.h>
#include "../../../src/ara/ucm/secure_boot_manager.h"
#include "../../../src/ara/crypto/rsa_provider.h"

namespace ara
{
        namespace ucm
        {
        namespace
        {
            CertificateEntry MakeValidRoot(
                const ara::crypto::RsaKeyPair &rootKeyPair)
            {
                CertificateEntry _root;
                _root.SubjectName = "RootCA";
                _root.IssuerName = "RootCA";
                _root.PublicKey = rootKeyPair.PublicKeyDer;
                _root.NotBefore = std::chrono::system_clock::now() -
                                  std::chrono::hours(1);
                _root.NotAfter = std::chrono::system_clock::now() +
                                 std::chrono::hours(24 * 365);
                return _root;
            }

            CertificateEntry MakeSignedCertificate(
                const std::string &subject,
                const std::string &issuer,
                const ara::crypto::RsaKeyPair &subjectKeyPair,
                const ara::crypto::RsaKeyPair &issuerKeyPair)
            {
                CertificateEntry _cert;
                _cert.SubjectName = subject;
                _cert.IssuerName = issuer;
                _cert.PublicKey = subjectKeyPair.PublicKeyDer;
                auto _signature = ara::crypto::RsaSign(
                    _cert.PublicKey, issuerKeyPair.PrivateKeyDer);
                EXPECT_TRUE(_signature.HasValue());
                if (_signature.HasValue())
                {
                    _cert.Signature = _signature.Value();
                }
                _cert.NotBefore = std::chrono::system_clock::now() -
                                  std::chrono::hours(1);
                _cert.NotAfter = std::chrono::system_clock::now() +
                                 std::chrono::hours(24 * 365);
                return _cert;
            }
        }

	        TEST(SecureBootManagerTest, VerifyBootChainWithoutRootFails)
	        {
            SecureBootManager _mgr;
            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kChainIncomplete);
        }

	        TEST(SecureBootManagerTest, SetRootAndVerifyBootChain)
	        {
            auto _rootKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_rootKeyPair.HasValue());

	            SecureBootManager _mgr;
	            _mgr.SetRootCertificate(MakeValidRoot(_rootKeyPair.Value()));

            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kVerified);
        }

	        TEST(SecureBootManagerTest, AddIntermediateCertificate)
	        {
            auto _rootKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
            auto _interKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_rootKeyPair.HasValue());
            ASSERT_TRUE(_interKeyPair.HasValue());

	            SecureBootManager _mgr;
	            _mgr.SetRootCertificate(MakeValidRoot(_rootKeyPair.Value()));
	            _mgr.AddIntermediateCertificate(
                MakeSignedCertificate(
                    "IntermediateCA",
                    "RootCA",
                    _interKeyPair.Value(),
                    _rootKeyPair.Value()));

            auto _report = _mgr.VerifyBootChain();
            EXPECT_EQ(_report.Result, BootVerifyResult::kVerified);
        }

	        TEST(SecureBootManagerTest, RevokeCertificate)
	        {
            auto _rootKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
            auto _badKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
            ASSERT_TRUE(_rootKeyPair.HasValue());
            ASSERT_TRUE(_badKeyPair.HasValue());

	            SecureBootManager _mgr;
	            _mgr.SetRootCertificate(MakeValidRoot(_rootKeyPair.Value()));
	            _mgr.AddIntermediateCertificate(
                MakeSignedCertificate(
                    "BadCert",
                    "RootCA",
                    _badKeyPair.Value(),
                    _rootKeyPair.Value()));

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

	        TEST(SecureBootManagerTest, VerifyPackageSignatureUsesSignerPublicKey)
	        {
	            auto _rootKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
	            auto _signerKeyPair = ara::crypto::GenerateRsaKeyPair(2048U);
	            ASSERT_TRUE(_rootKeyPair.HasValue());
	            ASSERT_TRUE(_signerKeyPair.HasValue());

	            SecureBootManager _mgr;
	            _mgr.SetRootCertificate(MakeValidRoot(_rootKeyPair.Value()));
	            _mgr.AddIntermediateCertificate(
                MakeSignedCertificate(
                    "Signer",
                    "RootCA",
                    _signerKeyPair.Value(),
                    _rootKeyPair.Value()));

	            std::vector<uint8_t> _digest{0x10, 0x20, 0x30, 0x40};
	            auto _signature = ara::crypto::RsaSign(
	                _digest, _signerKeyPair.Value().PrivateKeyDer);
            ASSERT_TRUE(_signature.HasValue());

            auto _ok = _mgr.VerifyPackageSignature(
                _digest, _signature.Value(), "Signer");
            EXPECT_EQ(BootVerifyResult::kVerified, _ok.Result);

            auto _badSignature = _signature.Value();
            _badSignature.front() ^= 0xFFU;
            auto _bad = _mgr.VerifyPackageSignature(
                _digest, _badSignature, "Signer");
            EXPECT_EQ(BootVerifyResult::kSignatureInvalid, _bad.Result);
        }
    }
}
