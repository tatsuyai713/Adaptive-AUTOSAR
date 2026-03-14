#include <gtest/gtest.h>
#include "../../../src/ara/com/runtime.h"

namespace ara
{
    namespace com
    {
        namespace runtime
        {
            class RuntimeTest : public ::testing::Test
            {
            protected:
                void TearDown() override
                {
                    // Ensure clean state for each test.
                    if (IsInitialized())
                    {
                        Deinitialize();
                    }
                }
            };

            TEST_F(RuntimeTest, InitiallyNotInitialized)
            {
                EXPECT_FALSE(IsInitialized());
            }

            TEST_F(RuntimeTest, InitializeSucceeds)
            {
                auto result = Initialize();
                EXPECT_TRUE(result.HasValue());
                EXPECT_TRUE(IsInitialized());
            }

            TEST_F(RuntimeTest, InitializeWithSpecifier)
            {
                core::InstanceSpecifier spec{"my_app"};
                auto result = Initialize(spec);
                EXPECT_TRUE(result.HasValue());
                EXPECT_TRUE(IsInitialized());
                EXPECT_EQ(GetApplicationName(), "my_app");
            }

            TEST_F(RuntimeTest, DoubleInitializeFails)
            {
                auto first = Initialize();
                EXPECT_TRUE(first.HasValue());

                auto second = Initialize();
                EXPECT_FALSE(second.HasValue());
            }

            TEST_F(RuntimeTest, DeinitializeSucceeds)
            {
                Initialize();
                auto result = Deinitialize();
                EXPECT_TRUE(result.HasValue());
                EXPECT_FALSE(IsInitialized());
            }

            TEST_F(RuntimeTest, DeinitializeWithoutInitFails)
            {
                auto result = Deinitialize();
                EXPECT_FALSE(result.HasValue());
            }

            TEST_F(RuntimeTest, ReinitializeAfterDeinit)
            {
                Initialize();
                Deinitialize();
                auto result = Initialize();
                EXPECT_TRUE(result.HasValue());
                EXPECT_TRUE(IsInitialized());
            }

            TEST_F(RuntimeTest, ApplicationNameDefault)
            {
                Initialize();
                EXPECT_EQ(GetApplicationName(), "default_application");
            }

            TEST_F(RuntimeTest, ApplicationNameClearedOnDeinit)
            {
                core::InstanceSpecifier spec{"test_app"};
                Initialize(spec);
                EXPECT_EQ(GetApplicationName(), "test_app");
                Deinitialize();
                EXPECT_TRUE(GetApplicationName().empty());
            }
        } // namespace runtime
    }
}
