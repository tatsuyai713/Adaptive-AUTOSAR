/// @file src/ara/com/secoc/secoc_pdu.cpp
/// @brief SecOC secured PDU processor implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./secoc_pdu.h"
#include <stdexcept>
#include <algorithm>

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            // -----------------------------------------------------------------------
            // Constructor
            // -----------------------------------------------------------------------
            SecOcPdu::SecOcPdu(
                const SecOcPduConfig &config,
                std::vector<uint8_t> key,
                MacFunction macFn,
                FreshnessManager *freshnessManager)
                : mConfig{config},
                  mKey{std::move(key)},
                  mMacFn{std::move(macFn)},
                  mFreshnessManager{freshnessManager}
            {
                if (mFreshnessManager == nullptr)
                {
                    throw std::invalid_argument("SecOcPdu: freshnessManager must not be null");
                }
                // Register PDU freshness counter
                mFreshnessManager->RegisterPdu(mConfig.dataId, mConfig.freshnessConfig);
            }

            // -----------------------------------------------------------------------
            // buildMacInput — concatenate DataID || Freshness || Payload
            // -----------------------------------------------------------------------
            std::vector<uint8_t> SecOcPdu::buildMacInput(
                const FreshnessValue &freshness,
                const std::vector<uint8_t> &payload) const
            {
                std::vector<uint8_t> input;
                input.reserve(2 + freshness.size() + payload.size());

                // DataID (big-endian)
                input.push_back(static_cast<uint8_t>(mConfig.dataId >> 8));
                input.push_back(static_cast<uint8_t>(mConfig.dataId & 0xFF));

                // Full freshness value
                input.insert(input.end(), freshness.begin(), freshness.end());

                // Payload
                input.insert(input.end(), payload.begin(), payload.end());

                return input;
            }

            // -----------------------------------------------------------------------
            // Protect — authenticate payload for transmission
            //
            // Wire format: | payload | truncated_freshness | truncated_MAC |
            // -----------------------------------------------------------------------
            ara::core::Result<std::vector<uint8_t>> SecOcPdu::Protect(
                const std::vector<uint8_t> &payload)
            {
                if (payload.empty())
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kInvalidPayloadLength));
                }

                // 1. Get current freshness value
                auto freshnessResult = mFreshnessManager->GetFreshnessValue(mConfig.dataId);
                if (!freshnessResult.HasValue())
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        freshnessResult.Error());
                }
                const FreshnessValue &freshness = freshnessResult.Value();

                // 2. Compute MAC over (DataID || Freshness || Payload)
                const std::vector<uint8_t> macInput = buildMacInput(freshness, payload);
                const std::vector<uint8_t> mac = mMacFn(mKey, macInput);

                if (mac.size() < mConfig.truncatedMacLength)
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kTruncatedMacFailed));
                }

                // 3. Build secured PDU
                std::vector<uint8_t> secured;
                secured.reserve(payload.size() +
                                mConfig.truncatedFreshnessLength +
                                mConfig.truncatedMacLength);

                // Payload
                secured.insert(secured.end(), payload.begin(), payload.end());

                // Truncated freshness (least significant bytes, little-endian order)
                const uint8_t truncFreshLen = std::min(
                    mConfig.truncatedFreshnessLength,
                    static_cast<uint8_t>(freshness.size()));
                secured.insert(secured.end(),
                               freshness.begin(),
                               freshness.begin() + truncFreshLen);

                // Truncated MAC (first N bytes of full MAC)
                secured.insert(secured.end(),
                               mac.begin(),
                               mac.begin() + mConfig.truncatedMacLength);

                // 4. Increment freshness counter
                auto incrResult = mFreshnessManager->IncrementCounter(mConfig.dataId);
                if (!incrResult.HasValue())
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        incrResult.Error());
                }

                return secured;
            }

            // -----------------------------------------------------------------------
            // Verify — authenticate received secured PDU
            //
            // Expected wire format: | payload | truncated_freshness | truncated_MAC |
            // -----------------------------------------------------------------------
            ara::core::Result<std::vector<uint8_t>> SecOcPdu::Verify(
                const std::vector<uint8_t> &securedPdu)
            {
                const std::size_t minLen =
                    mConfig.truncatedFreshnessLength + mConfig.truncatedMacLength + 1;

                if (securedPdu.size() < minLen)
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kInvalidPayloadLength));
                }

                // Extract MAC (last truncatedMacLength bytes)
                const std::size_t macStart = securedPdu.size() - mConfig.truncatedMacLength;
                const std::vector<uint8_t> receivedMac(
                    securedPdu.begin() + static_cast<std::ptrdiff_t>(macStart),
                    securedPdu.end());

                // Extract truncated freshness
                const std::size_t freshStart = macStart - mConfig.truncatedFreshnessLength;
                const FreshnessValue receivedTruncFresh(
                    securedPdu.begin() + static_cast<std::ptrdiff_t>(freshStart),
                    securedPdu.begin() + static_cast<std::ptrdiff_t>(macStart));

                // Extract payload
                const std::vector<uint8_t> payload(
                    securedPdu.begin(),
                    securedPdu.begin() + static_cast<std::ptrdiff_t>(freshStart));

                // Expand truncated freshness to full width using stored counter as upper bits
                auto currentFreshnessResult = mFreshnessManager->GetFreshnessValue(mConfig.dataId);
                if (!currentFreshnessResult.HasValue())
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        currentFreshnessResult.Error());
                }
                FreshnessValue fullFreshness = currentFreshnessResult.Value();

                // Overlay received truncated bytes (lower bytes) onto stored counter (upper bytes)
                for (std::size_t i = 0; i < receivedTruncFresh.size() && i < fullFreshness.size(); ++i)
                {
                    fullFreshness[i] = receivedTruncFresh[i];
                }

                // Verify freshness monotonicity
                auto verifyResult = mFreshnessManager->VerifyAndUpdate(mConfig.dataId, fullFreshness);
                if (!verifyResult.HasValue())
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        verifyResult.Error());
                }

                // Compute expected MAC
                const std::vector<uint8_t> macInput = buildMacInput(fullFreshness, payload);
                const std::vector<uint8_t> expectedMac = mMacFn(mKey, macInput);

                if (expectedMac.size() < mConfig.truncatedMacLength)
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kTruncatedMacFailed));
                }

                // Constant-time comparison of truncated MAC
                bool macMatch = true;
                for (uint8_t i = 0; i < mConfig.truncatedMacLength; ++i)
                {
                    macMatch &= (receivedMac[i] == expectedMac[i]);
                }

                if (!macMatch)
                {
                    return ara::core::Result<std::vector<uint8_t>>::FromError(
                        MakeErrorCode(SecOcErrc::kAuthenticationFailed));
                }

                // Return authenticated payload
                return payload;
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara
