/// @file src/ara/ucm/secure_boot_manager.h
/// @brief Secure boot verification chain and TPM integration for UCM.
/// @details Implements software package signature verification using a chain
///          of trust from a root CA through intermediate certificates.

#ifndef SECURE_BOOT_MANAGER_H
#define SECURE_BOOT_MANAGER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        /// @brief TPM attestation quote type.
        enum class TpmQuoteType : uint8_t
        {
            kNone = 0,
            kSoftware = 1,   ///< Software-emulated TPM (for testing).
            kHardwareTpm20 = 2 ///< Hardware TPM 2.0.
        };

        /// @brief Result of a boot chain verification.
        enum class BootVerifyResult : uint8_t
        {
            kVerified = 0,
            kSignatureInvalid = 1,
            kCertificateExpired = 2,
            kCertificateRevoked = 3,
            kChainIncomplete = 4,
            kHashMismatch = 5,
            kTpmAttestationFailed = 6
        };

        /// @brief Certificate entry in the verification chain.
        struct CertificateEntry
        {
            std::string SubjectName;
            std::string IssuerName;
            std::vector<uint8_t> PublicKey;
            std::vector<uint8_t> Signature;
            std::chrono::system_clock::time_point NotBefore;
            std::chrono::system_clock::time_point NotAfter;
            bool IsRevoked{false};
        };

        /// @brief Verification result with detailed information.
        struct BootVerificationReport
        {
            BootVerifyResult Result{BootVerifyResult::kChainIncomplete};
            std::string Details;
            size_t ChainDepth{0};
            std::string VerifiedSubject;
        };

        /// @brief Secure boot manager with chain-of-trust verification.
        /// @details Verifies software package signatures using a certificate
        ///          chain from root CA → intermediate → package signer.
        class SecureBootManager
        {
        public:
            SecureBootManager();
            ~SecureBootManager() = default;

            SecureBootManager(const SecureBootManager &) = delete;
            SecureBootManager &operator=(const SecureBootManager &) = delete;

            /// @brief Set the root CA certificate.
            void SetRootCertificate(CertificateEntry rootCert);

            /// @brief Add an intermediate certificate to the chain.
            void AddIntermediateCertificate(CertificateEntry cert);

            /// @brief Verify a software package signature against the chain.
            BootVerificationReport VerifyPackageSignature(
                const std::vector<uint8_t> &packageDigest,
                const std::vector<uint8_t> &signature,
                const std::string &signerSubject) const;

            /// @brief Verify boot chain integrity (root → all intermediates).
            BootVerificationReport VerifyBootChain() const;

            /// @brief Set the TPM quote type for attestation.
            void SetTpmQuoteType(TpmQuoteType type);

            /// @brief Get the configured TPM quote type.
            TpmQuoteType GetTpmQuoteType() const noexcept;

            /// @brief Request TPM attestation for measured boot.
            BootVerificationReport RequestTpmAttestation(
                const std::vector<uint8_t> &pcrValues) const;

            /// @brief Revoke a certificate by subject name.
            bool RevokeCertificate(const std::string &subjectName);

            /// @brief Get the number of certificates in the chain.
            size_t ChainSize() const;

            /// @brief Check if a specific subject has a valid certificate.
            bool HasValidCertificate(const std::string &subjectName) const;

        private:
            mutable std::mutex mMutex;
            CertificateEntry mRootCert;
            std::vector<CertificateEntry> mIntermediateCerts;
            TpmQuoteType mTpmType{TpmQuoteType::kSoftware};

            const CertificateEntry *FindCertificate(
                const std::string &subjectName) const;
            bool VerifyCertificateValidity(
                const CertificateEntry &cert) const;
            bool VerifySignatureChain(
                const std::string &subjectName) const;
        };
    }
}

#endif
