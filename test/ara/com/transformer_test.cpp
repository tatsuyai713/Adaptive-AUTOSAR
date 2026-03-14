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
    }
}
