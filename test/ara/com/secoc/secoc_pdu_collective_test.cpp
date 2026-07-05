#include <gtest/gtest.h>
#include "../../../../src/ara/com/secoc/secoc_pdu_collective.h"

namespace ara
{
    namespace com
    {
        namespace secoc
        {
            TEST(SecOcPduCollectiveTest, VerifyAllCallbackCanReenterAndMutate)
            {
                SecOcPduCollective collective;
                collective.SetOverrideStatus(SecOcOverrideStatus::kSkipVerify);

                std::vector<VerificationStatusIndication> indications;
                collective.SetVerificationStatusCallback(
                    [&collective, &indications](
                        const VerificationStatusIndication &indication)
                    {
                        EXPECT_EQ(collective.GetOverrideStatus(),
                                  SecOcOverrideStatus::kSkipVerify);
                        collective.SetOverrideStatus(SecOcOverrideStatus::kSkipAll);
                        collective.AddPdu(0x200U, nullptr);
                        indications.push_back(indication);
                    });

                const std::map<PduId, std::vector<std::uint8_t>> securedPdus{
                    {0x100U, {0x01U, 0x02U, 0x03U}}};

                auto result = collective.VerifyAll(securedPdus);
                ASSERT_TRUE(result.HasValue());
                ASSERT_EQ(indications.size(), 1U);
                EXPECT_EQ(indications[0].pduId, 0x100U);
                EXPECT_EQ(indications[0].result, VerificationResult::kPass);
                EXPECT_EQ(result.Value().at(0x100U), securedPdus.at(0x100U));
                EXPECT_EQ(collective.GetOverrideStatus(),
                          SecOcOverrideStatus::kSkipAll);
            }

        } // namespace secoc
    }     // namespace com
} // namespace ara
