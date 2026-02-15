#include <gtest/gtest.h>
#include "../../../src/ara/core/initialization.h"

namespace ara
{
    namespace core
    {
        TEST(InitializationTest, Initialize)
        {
            Result<void> _result = Initialize();
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(InitializationTest, DoubleInitialize)
        {
            Result<void> _result1 = Initialize();
            EXPECT_TRUE(_result1.HasValue());

            Result<void> _result2 = Initialize();
            EXPECT_TRUE(_result2.HasValue());
        }

        TEST(InitializationTest, Deinitialize)
        {
            Initialize();
            Result<void> _result = Deinitialize();
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(InitializationTest, DeinitializeWithoutInitialize)
        {
            Deinitialize(); // Reset state
            Result<void> _result = Deinitialize();
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(InitializationTest, InitializeAndDeinitialize)
        {
            Result<void> _initResult = Initialize();
            EXPECT_TRUE(_initResult.HasValue());

            Result<void> _deinitResult = Deinitialize();
            EXPECT_TRUE(_deinitResult.HasValue());
        }

        TEST(InitializationTest, IsInitializedReflectsRuntimeState)
        {
            Deinitialize();
            EXPECT_FALSE(IsInitialized());

            Result<void> _initResult = Initialize();
            EXPECT_TRUE(_initResult.HasValue());
            EXPECT_TRUE(IsInitialized());

            Result<void> _deinitResult = Deinitialize();
            EXPECT_TRUE(_deinitResult.HasValue());
            EXPECT_FALSE(IsInitialized());
        }
    }
}
