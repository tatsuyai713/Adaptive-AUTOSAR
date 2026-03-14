/// @file src/ara/iam/policy_signer.h
/// @brief Declarations for cryptographic policy signing and verification.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// `PolicySigner` provides tamper detection for `AccessControl` policy files
/// by attaching an ECDSA (P-256) detached signature alongside the serialised
/// policy payload.  The signing key is stored in an `ara::crypto::KeySlot`.
///
/// Typical workflow:
///   1. Create/load an EC key pair via `ara::crypto::GenerateEcKeyPair`.
///   2. Call `PolicySigner::SignPolicy(content, privateKeyDer, outSigFile)`.
///   3. Distribute the policy file together with the `.sig` file.
///   4. Before loading: call `PolicySigner::VerifyPolicy(content, publicKeyDer, sigFile)`.

#ifndef POLICY_SIGNER_H
#define POLICY_SIGNER_H

#include <string>
#include <vector>
#include "../core/result.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Provides ECDSA-based signing and verification for IAM policy files.
        ///
        /// All operations are stateless free-function style; no persistent state is held.
        class PolicySigner
        {
        public:
            PolicySigner() = delete; ///< Non-instantiable utility class.

            /// @brief Sign arbitrary policy content and write the signature to a file.
            /// @param content   Raw policy bytes to sign (e.g. the result of SaveToFile).
            /// @param privateKeyDer DER-encoded EC private key (from GenerateEcKeyPair).
            /// @param signatureFilePath Output path for the detached binary signature.
            /// @returns Void result or IAM domain error on failure.
            static core::Result<void> SignPolicy(
                const std::vector<std::uint8_t> &content,
                const std::vector<std::uint8_t> &privateKeyDer,
                const std::string &signatureFilePath);

            /// @brief Verify a previously signed policy.
            /// @param content       Raw policy bytes (identical to those that were signed).
            /// @param publicKeyDer  DER-encoded EC public key (from GenerateEcKeyPair).
            /// @param signatureFilePath Path to the detached binary signature file.
            /// @returns `true` if the signature is valid, `false` if invalid, or error.
            static core::Result<bool> VerifyPolicy(
                const std::vector<std::uint8_t> &content,
                const std::vector<std::uint8_t> &publicKeyDer,
                const std::string &signatureFilePath);

            /// @brief Convenience overload: sign a policy file by path.
            /// @param policyFilePath  Path to the policy file to sign.
            /// @param privateKeyDer   DER-encoded EC private key.
            /// @param signatureFilePath Output path for the detached signature.
            /// @returns Void result or IAM domain error.
            static core::Result<void> SignPolicyFile(
                const std::string &policyFilePath,
                const std::vector<std::uint8_t> &privateKeyDer,
                const std::string &signatureFilePath);

            /// @brief Convenience overload: verify a policy file by path.
            /// @param policyFilePath  Path to the policy file.
            /// @param publicKeyDer    DER-encoded EC public key.
            /// @param signatureFilePath Path to the detached signature file.
            /// @returns `true` if valid, `false` if invalid, or error.
            static core::Result<bool> VerifyPolicyFile(
                const std::string &policyFilePath,
                const std::vector<std::uint8_t> &publicKeyDer,
                const std::string &signatureFilePath);
        };
    }
}

#endif // POLICY_SIGNER_H
