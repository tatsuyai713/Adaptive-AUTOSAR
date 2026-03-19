#include <gtest/gtest.h>
#include "../../../src/ara/com/transformer.h"

namespace ara
{
    namespace com
    {
        TEST(TransformerTypeTest, EnumValues)
        {
            EXPECT_EQ(static_cast<uint8_t>(TransformerType::kSerialization), 0U);
            EXPECT_EQ(static_cast<uint8_t>(TransformerType::kProtection), 1U);
            EXPECT_EQ(static_cast<uint8_t>(TransformerType::kSecurity), 2U);
            EXPECT_EQ(static_cast<uint8_t>(TransformerType::kCompression), 3U);
            EXPECT_EQ(static_cast<uint8_t>(TransformerType::kCustom), 0xFFU);
        }

        TEST(TransformerConfigTest, DefaultValues)
        {
            TransformerConfig cfg;
            EXPECT_EQ(cfg.Type, TransformerType::kSerialization);
            EXPECT_TRUE(cfg.Name.empty());
            EXPECT_TRUE(cfg.IsMandatory);
            EXPECT_TRUE(cfg.ConfigData.empty());
        }

        TEST(IdentityTransformerTest, TransformPassthrough)
        {
            IdentityTransformer tf;
            std::vector<uint8_t> input{0x01, 0x02, 0x03};
            auto result = tf.Transform(input);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), input);
        }

        TEST(IdentityTransformerTest, InverseTransformPassthrough)
        {
            IdentityTransformer tf;
            std::vector<uint8_t> input{0xAA, 0xBB};
            auto result = tf.InverseTransform(input);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), input);
        }

        TEST(IdentityTransformerTest, GetType)
        {
            IdentityTransformer tf;
            EXPECT_EQ(tf.GetType(), TransformerType::kSerialization);
        }

        TEST(IdentityTransformerTest, EmptyInput)
        {
            IdentityTransformer tf;
            std::vector<uint8_t> empty;
            auto result = tf.Transform(empty);
            ASSERT_TRUE(result.HasValue());
            EXPECT_TRUE(result.Value().empty());
        }

        TEST(TransformerChainTest, EmptyChain)
        {
            TransformerChain chain;
            EXPECT_TRUE(chain.Empty());
            EXPECT_EQ(chain.Size(), 0U);
        }

        TEST(TransformerChainTest, AddTransformer)
        {
            TransformerChain chain;
            chain.Add(std::make_shared<IdentityTransformer>());
            EXPECT_FALSE(chain.Empty());
            EXPECT_EQ(chain.Size(), 1U);
        }

        TEST(TransformerChainTest, AddNullptr)
        {
            TransformerChain chain;
            chain.Add(nullptr);
            EXPECT_TRUE(chain.Empty());
        }

        TEST(TransformerChainTest, TransformThroughChain)
        {
            TransformerChain chain;
            chain.Add(std::make_shared<IdentityTransformer>());
            chain.Add(std::make_shared<IdentityTransformer>());

            std::vector<uint8_t> input{0x10, 0x20, 0x30};
            auto result = chain.Transform(input);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), input);
        }

        TEST(TransformerChainTest, InverseTransformThroughChain)
        {
            TransformerChain chain;
            chain.Add(std::make_shared<IdentityTransformer>());
            chain.Add(std::make_shared<IdentityTransformer>());

            std::vector<uint8_t> input{0xA0, 0xB0};
            auto result = chain.InverseTransform(input);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), input);
        }

        TEST(TransformerChainTest, ClearChain)
        {
            TransformerChain chain;
            chain.Add(std::make_shared<IdentityTransformer>());
            EXPECT_EQ(chain.Size(), 1U);
            chain.Clear();
            EXPECT_TRUE(chain.Empty());
        }

        TEST(TransformerChainTest, EmptyChainTransform)
        {
            TransformerChain chain;
            std::vector<uint8_t> input{0x01};
            auto result = chain.Transform(input);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), input);
        }

        TEST(DefaultTransformerFactoryTest, CreatesIdentity)
        {
            DefaultTransformerFactory factory;
            TransformerConfig cfg;
            cfg.Name = "test";
            auto result = factory.Create(cfg);
            ASSERT_TRUE(result.HasValue());
            EXPECT_NE(result.Value(), nullptr);
        }

        TEST(DefaultTransformerFactoryTest, CreatedTransformerWorks)
        {
            DefaultTransformerFactory factory;
            auto result = factory.Create(TransformerConfig{});
            ASSERT_TRUE(result.HasValue());

            std::vector<uint8_t> data{0xDE, 0xAD};
            auto transformed = result.Value()->Transform(data);
            ASSERT_TRUE(transformed.HasValue());
            EXPECT_EQ(transformed.Value(), data);
        }

        // ── SomeIpTransformer tests (SWS_CM_00710) ──

        TEST(SomeIpTransformerTest, GetType)
        {
            SomeIpTransformer tf(0x1234, 0x0001);
            EXPECT_EQ(tf.GetType(), TransformerType::kSerialization);
        }

        TEST(SomeIpTransformerTest, GetServiceAndMethodId)
        {
            SomeIpTransformer tf(0x1234, 0x8001);
            EXPECT_EQ(tf.GetServiceId(), 0x1234);
            EXPECT_EQ(tf.GetMethodId(), 0x8001);
        }

        TEST(SomeIpTransformerTest, TransformAddsHeader)
        {
            SomeIpTransformer tf(0x0100, 0x0200);
            std::vector<uint8_t> payload{0xAA, 0xBB, 0xCC};
            auto result = tf.Transform(payload);
            ASSERT_TRUE(result.HasValue());

            auto &output = result.Value();
            // 8 byte header + 3 byte payload
            EXPECT_EQ(output.size(), 11U);
            // ServiceId = 0x0100 in big-endian
            EXPECT_EQ(output[0], 0x01);
            EXPECT_EQ(output[1], 0x00);
            // MethodId = 0x0200 in big-endian
            EXPECT_EQ(output[2], 0x02);
            EXPECT_EQ(output[3], 0x00);
            // Length = 3 in big-endian
            EXPECT_EQ(output[4], 0x00);
            EXPECT_EQ(output[5], 0x00);
            EXPECT_EQ(output[6], 0x00);
            EXPECT_EQ(output[7], 0x03);
            // Payload preserved
            EXPECT_EQ(output[8], 0xAA);
            EXPECT_EQ(output[9], 0xBB);
            EXPECT_EQ(output[10], 0xCC);
        }

        TEST(SomeIpTransformerTest, InverseTransformStripsHeader)
        {
            SomeIpTransformer tf(0x0100, 0x0200);
            std::vector<uint8_t> payload{0xAA, 0xBB, 0xCC};

            auto transformed = tf.Transform(payload);
            ASSERT_TRUE(transformed.HasValue());

            auto inverse = tf.InverseTransform(transformed.Value());
            ASSERT_TRUE(inverse.HasValue());
            EXPECT_EQ(inverse.Value(), payload);
        }

        TEST(SomeIpTransformerTest, InverseTransformTooShortReturnsError)
        {
            SomeIpTransformer tf(0x0100, 0x0200);
            std::vector<uint8_t> tooShort{0x01, 0x02, 0x03};
            auto result = tf.InverseTransform(tooShort);
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SomeIpTransformerTest, InverseTransformInconsistentLengthReturnsError)
        {
            SomeIpTransformer tf(0x0100, 0x0200);
            // Header says length=100 but only 2 bytes follow
            std::vector<uint8_t> bad{
                0x01, 0x00, 0x02, 0x00,
                0x00, 0x00, 0x00, 0x64,
                0xAA, 0xBB};
            auto result = tf.InverseTransform(bad);
            EXPECT_FALSE(result.HasValue());
        }

        TEST(SomeIpTransformerTest, TransformEmptyPayload)
        {
            SomeIpTransformer tf(0x0100, 0x0200);
            std::vector<uint8_t> empty;
            auto result = tf.Transform(empty);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value().size(), 8U);

            auto inverse = tf.InverseTransform(result.Value());
            ASSERT_TRUE(inverse.HasValue());
            EXPECT_TRUE(inverse.Value().empty());
        }

        TEST(SomeIpTransformerTest, RoundtripInChain)
        {
            TransformerChain chain;
            chain.Add(std::make_shared<SomeIpTransformer>(0x1234, 0x8001));

            std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04};
            auto transformed = chain.Transform(data);
            ASSERT_TRUE(transformed.HasValue());
            EXPECT_EQ(transformed.Value().size(), 12U);

            auto original = chain.InverseTransform(transformed.Value());
            ASSERT_TRUE(original.HasValue());
            EXPECT_EQ(original.Value(), data);
        }
    }
}
