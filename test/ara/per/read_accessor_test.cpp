#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <string>
#include "../../../src/ara/per/read_accessor.h"
#include "../../../src/ara/per/read_write_accessor.h"

namespace ara
{
    namespace per
    {
        static const std::string cTestFile{"/tmp/ara_per_test_accessor.dat"};

        class ReadAccessorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::remove(cTestFile.c_str());
                // Write test data
                std::ofstream out(cTestFile, std::ios::binary);
                const char data[] = "Hello, AUTOSAR!";
                out.write(data, sizeof(data) - 1); // exclude null terminator
                out.close();
            }

            void TearDown() override
            {
                std::remove(cTestFile.c_str());
            }
        };

        TEST_F(ReadAccessorTest, ReadData)
        {
            ReadAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            std::uint8_t buffer[16] = {};
            auto result = accessor.Read(buffer, sizeof(buffer));
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 15U); // "Hello, AUTOSAR!" = 15 chars
            EXPECT_EQ(buffer[0], 'H');
            EXPECT_EQ(buffer[6], ' ');
        }

        TEST_F(ReadAccessorTest, GetSize)
        {
            ReadAccessor accessor(cTestFile);
            auto result = accessor.GetSize();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 15U);
        }

        TEST_F(ReadAccessorTest, Peek)
        {
            ReadAccessor accessor(cTestFile);
            std::uint8_t byte = 0;
            auto result = accessor.Peek(byte);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(byte, 'H');

            // Peek should not consume the byte
            auto result2 = accessor.Peek(byte);
            ASSERT_TRUE(result2.HasValue());
            EXPECT_EQ(byte, 'H');
        }

        TEST_F(ReadAccessorTest, ReadNonExistentFile)
        {
            ReadAccessor accessor("/tmp/ara_per_nonexistent_file.dat");
            EXPECT_FALSE(accessor.IsValid());
        }

        class ReadWriteAccessorTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                std::remove(cTestFile.c_str());
            }

            void TearDown() override
            {
                std::remove(cTestFile.c_str());
            }
        };

        TEST_F(ReadWriteAccessorTest, WriteAndRead)
        {
            // Write
            {
                ReadWriteAccessor accessor(cTestFile);
                ASSERT_TRUE(accessor.IsValid());

                const std::uint8_t data[] = {0xCA, 0xFE, 0xBA, 0xBE};
                auto writeResult = accessor.Write(data, sizeof(data));
                ASSERT_TRUE(writeResult.HasValue());
                EXPECT_EQ(writeResult.Value(), 4U);
                accessor.Sync();
            }

            // Read back
            {
                ReadAccessor accessor(cTestFile);
                ASSERT_TRUE(accessor.IsValid());

                std::uint8_t buffer[4] = {};
                auto result = accessor.Read(buffer, sizeof(buffer));
                ASSERT_TRUE(result.HasValue());
                EXPECT_EQ(result.Value(), 4U);
                EXPECT_EQ(buffer[0], 0xCA);
                EXPECT_EQ(buffer[3], 0xBE);
            }
        }

        TEST_F(ReadWriteAccessorTest, SetFileSize)
        {
            ReadWriteAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            const std::uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
            accessor.Write(data, sizeof(data));
            accessor.Sync();

            auto sizeResult = accessor.GetSize();
            ASSERT_TRUE(sizeResult.HasValue());
            EXPECT_EQ(sizeResult.Value(), 8U);

            auto truncResult = accessor.SetFileSize(4U);
            ASSERT_TRUE(truncResult.HasValue());

            sizeResult = accessor.GetSize();
            ASSERT_TRUE(sizeResult.HasValue());
            EXPECT_EQ(sizeResult.Value(), 4U);
        }

        TEST_F(ReadWriteAccessorTest, CreateNewFile)
        {
            // File doesn't exist yet - ReadWriteAccessor should create it
            ReadWriteAccessor accessor(cTestFile);
            EXPECT_TRUE(accessor.IsValid());
        }
    }
}
