/// @file src/ara/crypto/x509_provider.h
/// @brief X.509 certificate parsing and verification.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef X509_PROVIDER_H
#define X509_PROVIDER_H

#include <cstdint>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./crypto_error_domain.h"

namespace ara
{
    namespace crypto
    {
        /// @brief Parsed X.509 certificate information.
        struct X509CertificateInfo
        {
            std::string SubjectDn;
            std::string IssuerDn;
            std::string SerialNumber;
            std::uint64_t NotBeforeEpochSec;
            std::uint64_t NotAfterEpochSec;
            std::vector<std::uint8_t> PublicKeyDer;
            bool IsSelfSigned;
        };

        /// @brief Parse an X.509 certificate from PEM string.
        /// @param pemData PEM-encoded certificate string.
        /// @returns Parsed certificate info or a crypto domain error.
        core::Result<X509CertificateInfo> ParseX509Pem(
            const std::string &pemData);

        /// @brief Parse an X.509 certificate from DER bytes.
        /// @param derData DER-encoded certificate bytes.
        /// @returns Parsed certificate info or a crypto domain error.
        core::Result<X509CertificateInfo> ParseX509Der(
            const std::vector<std::uint8_t> &derData);

        /// @brief Verify an X.509 certificate chain.
        /// @param leafPem PEM-encoded leaf certificate.
        /// @param caCertsPem PEM-encoded CA certificates (trust anchors).
        /// @returns true if chain is valid, false if invalid, or crypto error.
        core::Result<bool> VerifyX509Chain(
            const std::string &leafPem,
            const std::vector<std::string> &caCertsPem);
    }
}

#endif
