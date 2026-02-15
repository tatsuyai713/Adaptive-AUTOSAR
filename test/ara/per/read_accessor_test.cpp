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

        TEST_F(ReadAccessorTest, SeekToBeginningAndReread)
        {
            ReadAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            // Read first 5 bytes
            std::uint8_t buffer[5] = {};
            auto result = accessor.Read(buffer, 5U);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 5U);
            EXPECT_EQ(buffer[0], 'H');

            // Seek back to beginning
            auto seekResult = accessor.Seek(0);
            ASSERT_TRUE(seekResult.HasValue());

            // Read again from beginning
            std::uint8_t buffer2[5] = {};
            auto result2 = accessor.Read(buffer2, 5U);
            ASSERT_TRUE(result2.HasValue());
            EXPECT_EQ(result2.Value(), 5U);
            EXPECT_EQ(buffer2[0], 'H');
        }

        TEST_F(ReadAccessorTest, SeekFromEnd)
        {
            ReadAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            // Seek to 4 bytes before end ("AR!")
            auto seekResult = accessor.Seek(-4, SeekOrigin::kEnd);
            ASSERT_TRUE(seekResult.HasValue());

            std::uint8_t buffer[4] = {};
            auto result = accessor.Read(buffer, 4U);
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(buffer[0], 'S');
            EXPECT_EQ(buffer[1], 'A');
            EXPECT_EQ(buffer[2], 'R');
            EXPECT_EQ(buffer[3], '!');
        }

        TEST_F(ReadAccessorTest, GetCurrentPositionAfterRead)
        {
            ReadAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            auto pos0 = accessor.GetCurrentPosition();
            ASSERT_TRUE(pos0.HasValue());
            EXPECT_EQ(pos0.Value(), 0U);

            std::uint8_t buffer[7] = {};
            accessor.Read(buffer, 7U);

            auto pos7 = accessor.GetCurrentPosition();
            ASSERT_TRUE(pos7.HasValue());
            EXPECT_EQ(pos7.Value(), 7U);
        }

        TEST_F(ReadAccessorTest, SeekFromCurrent)
        {
            ReadAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            // Read 5 bytes to advance position to 5
            std::uint8_t buf[5] = {};
            accessor.Read(buf, 5U);

            // Seek forward 2 from current (position 7)
            auto seekResult = accessor.Seek(2, SeekOrigin::kCurrent);
            ASSERT_TRUE(seekResult.HasValue());

            auto pos = accessor.GetCurrentPosition();
            ASSERT_TRUE(pos.HasValue());
            EXPECT_EQ(pos.Value(), 7U);

            // Read next byte: "Hello, AUTOSAR!" position 7 = 'A'
            std::uint8_t byte = 0;
            auto readResult = accessor.Read(&byte, 1U);
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(byte, 'A');
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

        TEST_F(ReadWriteAccessorTest, SeekAndWriteAtPosition)
        {
            ReadWriteAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            // Write initial data
            const std::uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
            accessor.Write(data, sizeof(data));
            accessor.Sync();

            // Seek to position 2 and overwrite
            auto seekResult = accessor.Seek(2);
            ASSERT_TRUE(seekResult.HasValue());

            const std::uint8_t overwrite[] = {0xAA, 0xBB};
            accessor.Write(overwrite, sizeof(overwrite));
            accessor.Sync();

            // Seek back to beginning and read all
            accessor.Seek(0);
            std::uint8_t buffer[8] = {};
            auto readResult = accessor.Read(buffer, 8U);
            ASSERT_TRUE(readResult.HasValue());
            EXPECT_EQ(buffer[0], 1);
            EXPECT_EQ(buffer[1], 2);
            EXPECT_EQ(buffer[2], 0xAA);
            EXPECT_EQ(buffer[3], 0xBB);
            EXPECT_EQ(buffer[4], 5);
        }

        TEST_F(ReadWriteAccessorTest, GetCurrentPositionAfterWrite)
        {
            ReadWriteAccessor accessor(cTestFile);
            ASSERT_TRUE(accessor.IsValid());

            auto pos0 = accessor.GetCurrentPosition();
            ASSERT_TRUE(pos0.HasValue());
            EXPECT_EQ(pos0.Value(), 0U);

            const std::uint8_t data[] = {10, 20, 30};
            accessor.Write(data, sizeof(data));

            auto pos3 = accessor.GetCurrentPosition();
            ASSERT_TRUE(pos3.HasValue());
            EXPECT_EQ(pos3.Value(), 3U);
        }

        TEST_F(ReadWriteAccessorTest, SeekOnInvalidStream)
        {
            ReadAccessor accessor("/tmp/ara_per_nonexistent_seek.dat");
            EXPECT_FALSE(accessor.IsValid());

            auto seekResult = accessor.Seek(0);
            EXPECT_FALSE(seekResult.HasValue());

            auto posResult = accessor.GetCurrentPosition();
            EXPECT_FALSE(posResult.HasValue());
        }
    }
}
