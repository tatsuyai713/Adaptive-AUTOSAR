/// @file src/ara/iam/biometric_interface.h
/// @brief Biometric sensor hardware abstraction for IAM.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef IAM_BIOMETRIC_INTERFACE_H
#define IAM_BIOMETRIC_INTERFACE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Type of biometric sensor.
        enum class BiometricType : uint8_t
        {
            kFingerprint = 0,
            kFace = 1,
            kIris = 2,
            kVoice = 3
        };

        /// @brief Result of a biometric verification attempt.
        enum class BiometricMatchResult : uint8_t
        {
            kMatch = 0,
            kNoMatch = 1,
            kTimeout = 2,
            kSensorError = 3,
            kTemplateNotFound = 4
        };

        /// @brief A stored biometric template for one identity.
        struct BiometricTemplate
        {
            std::string IdentityId;
            BiometricType Type{BiometricType::kFingerprint};
            /// @brief Opaque template data (e.g. minutiae-encoded).
            std::vector<uint8_t> TemplateData;
            std::chrono::steady_clock::time_point EnrolledAt;
        };

        /// @brief Result of a verification attempt.
        struct BiometricVerifyResult
        {
            BiometricMatchResult Result{BiometricMatchResult::kNoMatch};
            std::string MatchedIdentityId;
            double Confidence{0.0};
        };

        /// @brief Callback for async verification completion.
        using BiometricCallback =
            std::function<void(const BiometricVerifyResult &)>;

        /// @brief Biometric sensor abstraction layer for identity verification.
        class BiometricInterface
        {
        public:
            /// @param matchThreshold Confidence threshold [0..1] for a match.
            explicit BiometricInterface(double matchThreshold = 0.8);
            ~BiometricInterface() = default;

            /// @brief Enrol a biometric template for an identity.
            core::Result<void> Enrol(
                const std::string &identityId,
                BiometricType type,
                const std::vector<uint8_t> &templateData);

            /// @brief Remove a biometric template.
            core::Result<void> Revoke(const std::string &identityId);

            /// @brief Verify a sample against all enrolled templates.
            BiometricVerifyResult Verify(
                BiometricType type,
                const std::vector<uint8_t> &sample) const;

            /// @brief Check if an identity has an enrolled template.
            bool HasTemplate(const std::string &identityId) const;

            /// @brief Get all enrolled identity IDs.
            std::vector<std::string> GetEnrolledIdentities() const;

            /// @brief Get the number of enrolled templates.
            size_t GetTemplateCount() const;

        private:
            mutable std::mutex mMutex;
            double mThreshold;
            std::map<std::string, BiometricTemplate> mTemplates;

            /// @brief Compute a similarity score between two template data
            ///        blobs via normalized Hamming distance.
            double ComputeSimilarity(
                const std::vector<uint8_t> &a,
                const std::vector<uint8_t> &b) const;
        };
    }
}

#endif
