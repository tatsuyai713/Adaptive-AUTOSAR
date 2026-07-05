/// @file src/ara/ucm/secure_boot_manager.cpp
/// @brief Secure boot manager implementation.

#include "./secure_boot_manager.h"
#include "../crypto/rsa_provider.h"
#include <algorithm>

namespace ara
{
    namespace ucm
    {
        SecureBootManager::SecureBootManager()
        {
            mRootCert.SubjectName = "";
        }

        void SecureBootManager::SetRootCertificate(CertificateEntry rootCert)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRootCert = std::move(rootCert);
        }

        void SecureBootManager::AddIntermediateCertificate(
            CertificateEntry cert)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mIntermediateCerts.push_back(std::move(cert));
        }

        BootVerificationReport SecureBootManager::VerifyPackageSignature(
            const std::vector<uint8_t> &packageDigest,
            const std::vector<uint8_t> &signature,
            const std::string &signerSubject) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            BootVerificationReport report;

            if (mRootCert.SubjectName.empty())
            {
                report.Result = BootVerifyResult::kChainIncomplete;
                report.Details = "No root certificate configured";
                return report;
            }

            const CertificateEntry *signerCert = FindCertificate(signerSubject);
            if (!signerCert)
            {
                report.Result = BootVerifyResult::kChainIncomplete;
                report.Details = "Signer certificate not found: " + signerSubject;
                return report;
            }

            if (signerCert->IsRevoked)
            {
                report.Result = BootVerifyResult::kCertificateRevoked;
                report.Details = "Signer certificate revoked";
                return report;
            }

            if (!VerifyCertificateValidity(*signerCert))
            {
                report.Result = BootVerifyResult::kCertificateExpired;
                report.Details = "Signer certificate expired";
                return report;
            }

            if (signature.empty() || packageDigest.empty())
            {
                report.Result = BootVerifyResult::kSignatureInvalid;
                report.Details = "Empty signature or digest";
                return report;
            }

            if (signerCert->PublicKey.empty())
            {
                report.Result = BootVerifyResult::kSignatureInvalid;
                report.Details = "Signer public key is empty";
                return report;
            }

            if (!VerifySignatureChain(signerSubject))
            {
                report.Result = BootVerifyResult::kChainIncomplete;
                report.Details = "Certificate chain verification failed";
                return report;
            }

            auto verifyResult = ara::crypto::RsaVerify(
                packageDigest, signature, signerCert->PublicKey);
            if (!verifyResult.HasValue() || !verifyResult.Value())
            {
                report.Result = BootVerifyResult::kSignatureInvalid;
                report.Details = "Package signature cryptographic verification failed";
                return report;
            }

            // Chain depth: root + intermediates + signer
            report.ChainDepth = 1; // root
            for (const auto &ic : mIntermediateCerts)
            {
                if (!ic.IsRevoked)
                    ++report.ChainDepth;
            }

            report.Result = BootVerifyResult::kVerified;
            report.VerifiedSubject = signerSubject;
            report.Details = "Package signature verified";
            return report;
        }

        BootVerificationReport SecureBootManager::VerifyBootChain() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            BootVerificationReport report;

            if (mRootCert.SubjectName.empty())
            {
                report.Result = BootVerifyResult::kChainIncomplete;
                report.Details = "No root certificate";
                return report;
            }

            if (!VerifyCertificateValidity(mRootCert))
            {
                report.Result = BootVerifyResult::kCertificateExpired;
                report.Details = "Root certificate expired";
                return report;
            }

	            if (mRootCert.PublicKey.empty() ||
	                mRootCert.IssuerName != mRootCert.SubjectName)
	            {
	                report.Result = BootVerifyResult::kChainIncomplete;
	                report.Details = "Invalid root certificate";
	                return report;
	            }

	            size_t depth = 1;

	            for (const auto &cert : mIntermediateCerts)
	            {
                if (cert.IsRevoked)
                {
                    report.Result = BootVerifyResult::kCertificateRevoked;
                    report.Details = "Revoked: " + cert.SubjectName;
                    return report;
                }
                if (!VerifyCertificateValidity(cert))
                {
                    report.Result = BootVerifyResult::kCertificateExpired;
                    report.Details = "Expired: " + cert.SubjectName;
                    return report;
                }
	                const CertificateEntry *issuer = FindCertificate(cert.IssuerName);
	                if (issuer == nullptr || issuer->PublicKey.empty())
	                {
	                    report.Result = BootVerifyResult::kChainIncomplete;
	                    report.Details = "Issuer not found: " + cert.SubjectName;
	                    return report;
	                }
	                if (cert.PublicKey.empty() || cert.Signature.empty())
	                {
	                    report.Result = BootVerifyResult::kSignatureInvalid;
	                    report.Details = "Certificate signature missing: " + cert.SubjectName;
	                    return report;
	                }
	                auto verifyResult = ara::crypto::RsaVerify(
	                    cert.PublicKey, cert.Signature, issuer->PublicKey);
	                if (!verifyResult.HasValue() || !verifyResult.Value())
	                {
	                    report.Result = BootVerifyResult::kSignatureInvalid;
	                    report.Details = "Certificate signature invalid: " + cert.SubjectName;
	                    return report;
	                }
	                ++depth;
	            }

	            report.Result = BootVerifyResult::kVerified;
	            report.ChainDepth = depth;
	            report.VerifiedSubject = mIntermediateCerts.empty()
                    ? mRootCert.SubjectName
                    : mIntermediateCerts.back().SubjectName;
            report.Details = "Boot chain verified";
            return report;
        }

        void SecureBootManager::SetTpmQuoteType(TpmQuoteType type)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mTpmType = type;
        }

        TpmQuoteType SecureBootManager::GetTpmQuoteType() const noexcept
        {
            return mTpmType;
        }

        BootVerificationReport SecureBootManager::RequestTpmAttestation(
            const std::vector<uint8_t> &pcrValues) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            BootVerificationReport report;

            if (mTpmType == TpmQuoteType::kNone)
            {
                report.Result = BootVerifyResult::kTpmAttestationFailed;
                report.Details = "TPM not configured";
                return report;
            }

            if (pcrValues.empty())
            {
                report.Result = BootVerifyResult::kHashMismatch;
                report.Details = "Empty PCR values";
                return report;
            }

            // Software TPM simulation: accept non-zero PCR values
            bool allZero = std::all_of(pcrValues.begin(), pcrValues.end(),
                                       [](uint8_t v) { return v == 0; });
            if (allZero)
            {
                report.Result = BootVerifyResult::kTpmAttestationFailed;
                report.Details = "PCR values all zero — boot not measured";
                return report;
            }

            report.Result = BootVerifyResult::kVerified;
            report.Details = "TPM attestation passed";
            return report;
        }

        bool SecureBootManager::RevokeCertificate(
            const std::string &subjectName)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto &cert : mIntermediateCerts)
            {
                if (cert.SubjectName == subjectName)
                {
                    cert.IsRevoked = true;
                    return true;
                }
            }
            return false;
        }

        size_t SecureBootManager::ChainSize() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            size_t count = mRootCert.SubjectName.empty() ? 0 : 1;
            count += mIntermediateCerts.size();
            return count;
        }

        bool SecureBootManager::HasValidCertificate(
            const std::string &subjectName) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            const CertificateEntry *cert = FindCertificate(subjectName);
            if (!cert)
                return false;
            if (cert->IsRevoked)
                return false;
            return VerifyCertificateValidity(*cert);
        }

        const CertificateEntry *SecureBootManager::FindCertificate(
            const std::string &subjectName) const
        {
            if (mRootCert.SubjectName == subjectName)
                return &mRootCert;
            for (const auto &cert : mIntermediateCerts)
            {
                if (cert.SubjectName == subjectName)
                    return &cert;
            }
            return nullptr;
        }

        bool SecureBootManager::VerifyCertificateValidity(
            const CertificateEntry &cert) const
        {
            auto now = std::chrono::system_clock::now();
            if (cert.NotBefore.time_since_epoch().count() > 0 &&
                now < cert.NotBefore)
            {
                return false;
            }
            if (cert.NotAfter.time_since_epoch().count() > 0 &&
                now > cert.NotAfter)
            {
                return false;
            }
            return true;
        }

        bool SecureBootManager::VerifySignatureChain(
            const std::string &subjectName) const
        {
	            if (mRootCert.SubjectName.empty() ||
	                mRootCert.PublicKey.empty() ||
	                mRootCert.IssuerName != mRootCert.SubjectName ||
	                !VerifyCertificateValidity(mRootCert))
	            {
	                return false;
	            }

	            // Walk from signer up to root and verify every certificate signature.
	            std::string current = subjectName;
	            for (int depth = 0; depth < 10; ++depth)
	            {
	                if (current == mRootCert.SubjectName)
	                {
	                    return true;
	                }

	                const CertificateEntry *cert = FindCertificate(current);
	                if (!cert || cert->IsRevoked ||
	                    !VerifyCertificateValidity(*cert) ||
	                    cert->PublicKey.empty() ||
	                    cert->Signature.empty())
	                    return false;

	                const CertificateEntry *issuer = FindCertificate(cert->IssuerName);
	                if (issuer == nullptr || issuer->PublicKey.empty() ||
	                    issuer->IsRevoked ||
	                    !VerifyCertificateValidity(*issuer))
	                {
	                    return false;
	                }

	                auto verifyResult = ara::crypto::RsaVerify(
	                    cert->PublicKey, cert->Signature, issuer->PublicKey);
	                if (!verifyResult.HasValue() || !verifyResult.Value())
	                {
	                    return false;
	                }

	                current = issuer->SubjectName;
	            }
            return false;
        }
    }
}
