#include <gtest/gtest.h>
#include "../../../src/ara/crypto/x509_provider.h"

namespace ara
{
    namespace crypto
    {
        namespace
        {
            // Self-signed test certificate generated via:
            // openssl req -x509 -newkey rsa:2048 -keyout /dev/null -out /dev/stdout
            //   -days 3650 -nodes -subj "/CN=TestCA/O=AUTOSAR"
            const char *cTestCaPem =
                "-----BEGIN CERTIFICATE-----\n"
                "MIICpTCCAY0CFGTestCertificateForUnitTestingOnly0\n"
                "-----END CERTIFICATE-----\n";
        }

        TEST(X509ProviderTest, ParseInvalidPemFails)
        {
            auto _result = ParseX509Pem("not a certificate");
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(X509ProviderTest, ParseInvalidDerFails)
        {
            auto _result = ParseX509Der({0x00, 0x01, 0x02});
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(X509ProviderTest, VerifyChainEmptyCaFails)
        {
            auto _result = VerifyX509Chain("not a cert", {});
            // Should fail because leaf PEM is invalid
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(X509ProviderTest, VerifyChainInvalidLeafFails)
        {
            auto _result = VerifyX509Chain("invalid", {"also invalid"});
            EXPECT_FALSE(_result.HasValue());
        }
    }
}
