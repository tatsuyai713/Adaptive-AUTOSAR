/// @file src/ara/iam/policy_signer.cpp
/// @brief Implementation for cryptographic policy signing (ECDSA P-256, SHA-256).
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// Delegates all ECDSA operations to ara::crypto::EcdsaSign / EcdsaVerify
/// so that policy_signer.cpp does not directly depend on OpenSSL APIs and
/// remains compatible with both OpenSSL 1.1 and OpenSSL 3.x.

#include "./policy_signer.h"

#include <fstream>
#include <iterator>

#include "../crypto/ecdsa_provider.h"

namespace ara
{
    namespace iam
    {
        namespace
        {
            /// @brief Load raw bytes from a binary file.
            static core::Result<std::vector<std::uint8_t>> LoadBinaryFile(
                const std::string &path)
            {
                std::ifstream ifs(path, std::ios::binary);
                if (!ifs.is_open())
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(IamErrc::kPolicyStoreError));
                }
                std::vector<std::uint8_t> data(
                    (std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
                if (ifs.bad())
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(IamErrc::kPolicyStoreError));
                }
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(data));
            }

            /// @brief Write raw bytes to a binary file.
            static core::Result<void> SaveBinaryFile(
                const std::string &path,
                const std::vector<std::uint8_t> &data)
            {
                std::ofstream ofs(path, std::ios::binary);
                if (!ofs.is_open())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyStoreError));
                }
                if (!data.empty())
                {
                    ofs.write(
                        reinterpret_cast<const char *>(data.data()),
                        static_cast<std::streamsize>(data.size()));
                }
                if (!ofs.good())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyStoreError));
                }
                return core::Result<void>::FromValue();
            }
        }

        // -----------------------------------------------------------------------
        // SignPolicy
        // -----------------------------------------------------------------------
        core::Result<void> PolicySigner::SignPolicy(
            const std::vector<std::uint8_t> &content,
            const std::vector<std::uint8_t> &privateKeyDer,
            const std::string &signatureFilePath)
        {
            if (privateKeyDer.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            auto signResult = ara::crypto::EcdsaSign(
                content,
                privateKeyDer,
                ara::crypto::DigestAlgorithm::kSha256);

            if (!signResult.HasValue())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            return SaveBinaryFile(signatureFilePath, signResult.Value());
        }

        // -----------------------------------------------------------------------
        // VerifyPolicy
        // -----------------------------------------------------------------------
        core::Result<bool> PolicySigner::VerifyPolicy(
            const std::vector<std::uint8_t> &content,
            const std::vector<std::uint8_t> &publicKeyDer,
            const std::string &signatureFilePath)
        {
            if (publicKeyDer.empty())
            {
                return core::Result<bool>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            auto sigResult = LoadBinaryFile(signatureFilePath);
            if (!sigResult.HasValue())
            {
                return core::Result<bool>::FromError(sigResult.Error());
            }

            auto verifyResult = ara::crypto::EcdsaVerify(
                content,
                sigResult.Value(),
                publicKeyDer,
                ara::crypto::DigestAlgorithm::kSha256);

            if (!verifyResult.HasValue())
            {
                // Translate crypto error to IAM error
                return core::Result<bool>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            return core::Result<bool>::FromValue(verifyResult.Value());
        }

        // -----------------------------------------------------------------------
        // SignPolicyFile / VerifyPolicyFile (file path convenience overloads)
        // -----------------------------------------------------------------------
        core::Result<void> PolicySigner::SignPolicyFile(
            const std::string &policyFilePath,
            const std::vector<std::uint8_t> &privateKeyDer,
            const std::string &signatureFilePath)
        {
            auto contentResult = LoadBinaryFile(policyFilePath);
            if (!contentResult.HasValue())
            {
                return core::Result<void>::FromError(contentResult.Error());
            }
            return SignPolicy(contentResult.Value(), privateKeyDer, signatureFilePath);
        }

        core::Result<bool> PolicySigner::VerifyPolicyFile(
            const std::string &policyFilePath,
            const std::vector<std::uint8_t> &publicKeyDer,
            const std::string &signatureFilePath)
        {
            auto contentResult = LoadBinaryFile(policyFilePath);
            if (!contentResult.HasValue())
            {
                return core::Result<bool>::FromError(contentResult.Error());
            }
            return VerifyPolicy(contentResult.Value(), publicKeyDer, signatureFilePath);
        }
    }
}
