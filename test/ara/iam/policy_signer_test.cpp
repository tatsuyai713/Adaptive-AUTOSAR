#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "../../../src/ara/iam/policy_signer.h"
#include "../../../src/ara/crypto/ecdsa_provider.h"


namespace ara
{
    namespace iam
    {
        namespace
        {
            /// Helper: write bytes to a tmp file and return path.
            std::string WriteTmpFile(
                const std::string &name,
                const std::vector<std::uint8_t> &data)
            {
                std::string path{"/tmp/" + name};
                std::ofstream ofs(path, std::ios::binary);
                if (!data.empty())
                {
                    ofs.write(
                        reinterpret_cast<const char *>(data.data()),
                        static_cast<std::streamsize>(data.size()));
                }
                return path;
            }

            /// Helper: read bytes from a tmp file.
            std::vector<std::uint8_t> ReadTmpFile(const std::string &path)
            {
                std::ifstream ifs(path, std::ios::binary);
                return std::vector<std::uint8_t>(
                    (std::istreambuf_iterator<char>(ifs)),
                    std::istreambuf_iterator<char>());
            }
        }

        TEST(PolicySignerTest, SignAndVerifyValidPolicy)
        {
            // Generate an EC P-256 key pair
            auto keyResult = ara::crypto::GenerateEcKeyPair(
                ara::crypto::EllipticCurve::kP256);
            ASSERT_TRUE(keyResult.HasValue());

            const auto &keyPair = keyResult.Value();
            const std::vector<std::uint8_t> cContent{
                's', 'u', 'b', 'j', 'e', 'c', 't', '=', 'a', 'l', 'i', 'c', 'e'};

            const std::string sigFile{"/tmp/ara_policy_signer_test.sig"};

            // Sign
            auto signResult = PolicySigner::SignPolicy(
                cContent, keyPair.PrivateKeyDer, sigFile);
            ASSERT_TRUE(signResult.HasValue());

            // Verify with matching public key
            auto verifyResult = PolicySigner::VerifyPolicy(
                cContent, keyPair.PublicKeyDer, sigFile);
            ASSERT_TRUE(verifyResult.HasValue());
            EXPECT_TRUE(verifyResult.Value());

            std::remove(sigFile.c_str());
        }

        TEST(PolicySignerTest, VerifyWithTamperedContentFails)
        {
            auto keyResult = ara::crypto::GenerateEcKeyPair(
                ara::crypto::EllipticCurve::kP256);
            ASSERT_TRUE(keyResult.HasValue());
            const auto &keyPair = keyResult.Value();

            const std::vector<std::uint8_t> cOriginal{0x01, 0x02, 0x03};
            const std::vector<std::uint8_t> cTampered{0x01, 0x02, 0xFF};
            const std::string sigFile{"/tmp/ara_policy_signer_tamper.sig"};

            ASSERT_TRUE(PolicySigner::SignPolicy(
                cOriginal, keyPair.PrivateKeyDer, sigFile).HasValue());

            auto verifyResult = PolicySigner::VerifyPolicy(
                cTampered, keyPair.PublicKeyDer, sigFile);
            ASSERT_TRUE(verifyResult.HasValue());
            EXPECT_FALSE(verifyResult.Value());

            std::remove(sigFile.c_str());
        }

        TEST(PolicySignerTest, VerifyWithWrongPublicKeyFails)
        {
            auto key1 = ara::crypto::GenerateEcKeyPair(ara::crypto::EllipticCurve::kP256);
            auto key2 = ara::crypto::GenerateEcKeyPair(ara::crypto::EllipticCurve::kP256);
            ASSERT_TRUE(key1.HasValue());
            ASSERT_TRUE(key2.HasValue());

            const std::vector<std::uint8_t> cContent{0xAA, 0xBB, 0xCC};
            const std::string sigFile{"/tmp/ara_policy_signer_wrongkey.sig"};

            ASSERT_TRUE(PolicySigner::SignPolicy(
                cContent, key1.Value().PrivateKeyDer, sigFile).HasValue());

            // Verify with key2's public key (wrong key)
            auto verifyResult = PolicySigner::VerifyPolicy(
                cContent, key2.Value().PublicKeyDer, sigFile);
            ASSERT_TRUE(verifyResult.HasValue());
            EXPECT_FALSE(verifyResult.Value());

            std::remove(sigFile.c_str());
        }

        TEST(PolicySignerTest, SignWithEmptyPrivateKeyReturnsError)
        {
            const std::vector<std::uint8_t> cEmpty;
            const std::vector<std::uint8_t> cContent{0x01};
            const std::string sigFile{"/tmp/ara_policy_signer_empty.sig"};

            auto result = PolicySigner::SignPolicy(cContent, cEmpty, sigFile);
            EXPECT_FALSE(result.HasValue());
        }

        TEST(PolicySignerTest, VerifyWithMissingSigFileReturnsError)
        {
            auto keyResult = ara::crypto::GenerateEcKeyPair(
                ara::crypto::EllipticCurve::kP256);
            ASSERT_TRUE(keyResult.HasValue());

            const std::vector<std::uint8_t> cContent{0x01};
            auto result = PolicySigner::VerifyPolicy(
                cContent,
                keyResult.Value().PublicKeyDer,
                "/tmp/ara_policy_signer_nonexistent.sig");
            EXPECT_FALSE(result.HasValue());
        }

        TEST(PolicySignerTest, SignAndVerifyPolicyFileConvenienceOverload)
        {
            auto keyResult = ara::crypto::GenerateEcKeyPair(
                ara::crypto::EllipticCurve::kP256);
            ASSERT_TRUE(keyResult.HasValue());
            const auto &keyPair = keyResult.Value();

            const std::vector<std::uint8_t> cContent{
                'r', 'u', 'l', 'e', '=', 't', 'e', 's', 't'};
            const std::string policyFile{"/tmp/ara_policy_signer_file.txt"};
            const std::string sigFile{"/tmp/ara_policy_signer_file.sig"};

            // Write policy content to file
            {
                std::ofstream ofs(policyFile, std::ios::binary);
                ofs.write(
                    reinterpret_cast<const char *>(cContent.data()),
                    static_cast<std::streamsize>(cContent.size()));
            }

            ASSERT_TRUE(PolicySigner::SignPolicyFile(
                policyFile, keyPair.PrivateKeyDer, sigFile).HasValue());

            auto verifyResult = PolicySigner::VerifyPolicyFile(
                policyFile, keyPair.PublicKeyDer, sigFile);
            ASSERT_TRUE(verifyResult.HasValue());
            EXPECT_TRUE(verifyResult.Value());

            std::remove(policyFile.c_str());
            std::remove(sigFile.c_str());
        }
    }
}
