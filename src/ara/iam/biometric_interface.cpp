/// @file src/ara/iam/biometric_interface.cpp
/// @brief Implementation of BiometricInterface.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./biometric_interface.h"
#include <algorithm>
#include <cmath>

namespace ara
{
    namespace iam
    {
        BiometricInterface::BiometricInterface(double matchThreshold)
            : mThreshold{matchThreshold}
        {
        }

        core::Result<void> BiometricInterface::Enrol(
            const std::string &identityId,
            BiometricType type,
            const std::vector<uint8_t> &templateData)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (identityId.empty() || templateData.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }
            if (mTemplates.count(identityId))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }
            BiometricTemplate tmpl;
            tmpl.IdentityId = identityId;
            tmpl.Type = type;
            tmpl.TemplateData = templateData;
            tmpl.EnrolledAt = std::chrono::steady_clock::now();
            mTemplates[identityId] = std::move(tmpl);
            return {};
        }

        core::Result<void> BiometricInterface::Revoke(
            const std::string &identityId)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mTemplates.find(identityId);
            if (it == mTemplates.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }
            mTemplates.erase(it);
            return {};
        }

        BiometricVerifyResult BiometricInterface::Verify(
            BiometricType type,
            const std::vector<uint8_t> &sample) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            BiometricVerifyResult best;
            best.Result = BiometricMatchResult::kNoMatch;

            if (sample.empty())
            {
                best.Result = BiometricMatchResult::kSensorError;
                return best;
            }

            for (const auto &kv : mTemplates)
            {
                if (kv.second.Type != type) continue;

                double score = ComputeSimilarity(
                    kv.second.TemplateData, sample);
                if (score > best.Confidence)
                {
                    best.Confidence = score;
                    best.MatchedIdentityId = kv.first;
                }
            }

            if (best.Confidence >= mThreshold &&
                !best.MatchedIdentityId.empty())
            {
                best.Result = BiometricMatchResult::kMatch;
            }
            else if (best.MatchedIdentityId.empty())
            {
                best.Result = BiometricMatchResult::kTemplateNotFound;
            }

            return best;
        }

        bool BiometricInterface::HasTemplate(
            const std::string &identityId) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mTemplates.count(identityId) > 0;
        }

        std::vector<std::string> BiometricInterface::GetEnrolledIdentities()
            const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            std::vector<std::string> ids;
            ids.reserve(mTemplates.size());
            for (const auto &kv : mTemplates)
            {
                ids.push_back(kv.first);
            }
            return ids;
        }

        size_t BiometricInterface::GetTemplateCount() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mTemplates.size();
        }

        double BiometricInterface::ComputeSimilarity(
            const std::vector<uint8_t> &a,
            const std::vector<uint8_t> &b) const
        {
            if (a.empty() || b.empty()) return 0.0;

            // Normalized byte-wise comparison (Hamming-like distance).
            size_t minLen = std::min(a.size(), b.size());
            size_t maxLen = std::max(a.size(), b.size());
            uint32_t matchingBits = 0;
            uint32_t totalBits = static_cast<uint32_t>(maxLen * 8);

            for (size_t i = 0; i < minLen; ++i)
            {
                uint8_t xorVal = a[i] ^ b[i];
                // Count matching bits (8 - popcount of XOR).
                uint32_t diffBits = 0;
                uint8_t v = xorVal;
                while (v)
                {
                    diffBits += v & 1;
                    v >>= 1;
                }
                matchingBits += (8 - diffBits);
            }

            if (totalBits == 0) return 0.0;
            return static_cast<double>(matchingBits) /
                   static_cast<double>(totalBits);
        }
    }
}
