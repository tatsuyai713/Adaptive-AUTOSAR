#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../../src/ara/per/file_storage.h"

namespace ara
{
    namespace per
    {
        static const std::string cTestDir{"/tmp/ara_per_test_files"};

        class FileStorageTest : public ::testing::Test
        {
        protected:
            void SetUp() override
            {
                // Clean up test directory
                std::string cmd = "rm -rf " + cTestDir;
                std::system(cmd.c_str());
            }

            void TearDown() override
            {
                std::string cmd = "rm -rf " + cTestDir;
                std::system(cmd.c_str());
            }
        };

        TEST_F(FileStorageTest, CreateAndWriteFile)
        {
            FileStorage storage(cTestDir);

            auto result = storage.OpenFileReadWrite("test.bin");
            ASSERT_TRUE(result.HasValue());

            auto &accessor = result.Value();
            const std::uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
            auto writeResult = accessor->Write(data, sizeof(data));
            ASSERT_TRUE(writeResult.HasValue());
            EXPECT_EQ(writeResult.Value(), sizeof(data));
        }

        TEST_F(FileStorageTest, WriteAndReadBack)
        {
            FileStorage storage(cTestDir);

            // Write
            {
                auto result = storage.OpenFileReadWrite("data.bin");
                ASSERT_TRUE(result.HasValue());
                const std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
                result.Value()->Write(data, sizeof(data));
                result.Value()->Sync();
            }

            // Read back
            {
                auto result = storage.OpenFileReadOnly("data.bin");
                ASSERT_TRUE(result.HasValue());

                std::uint8_t buffer[4] = {};
                auto readResult = result.Value()->Read(buffer, sizeof(buffer));
                ASSERT_TRUE(readResult.HasValue());
                EXPECT_EQ(readResult.Value(), 4U);
                EXPECT_EQ(buffer[0], 0xDE);
                EXPECT_EQ(buffer[1], 0xAD);
                EXPECT_EQ(buffer[2], 0xBE);
                EXPECT_EQ(buffer[3], 0xEF);
            }
        }

        TEST_F(FileStorageTest, FileExists)
        {
            FileStorage storage(cTestDir);

            EXPECT_FALSE(storage.FileExists("nofile.dat"));

            auto result = storage.OpenFileReadWrite("exists.dat");
            ASSERT_TRUE(result.HasValue());

            EXPECT_TRUE(storage.FileExists("exists.dat"));
        }

        TEST_F(FileStorageTest, DeleteFile)
        {
            FileStorage storage(cTestDir);

            {
                auto result = storage.OpenFileReadWrite("todelete.dat");
                ASSERT_TRUE(result.HasValue());
                // let accessor go out of scope to close file
            }

            EXPECT_TRUE(storage.FileExists("todelete.dat"));

            auto delResult = storage.DeleteFile("todelete.dat");
            ASSERT_TRUE(delResult.HasValue());

            EXPECT_FALSE(storage.FileExists("todelete.dat"));
        }

        TEST_F(FileStorageTest, DeleteNonExistentFile)
        {
            FileStorage storage(cTestDir);

            auto result = storage.DeleteFile("nonexistent.dat");
            EXPECT_FALSE(result.HasValue());
        }

        TEST_F(FileStorageTest, GetAllFileNames)
        {
            FileStorage storage(cTestDir);

            storage.OpenFileReadWrite("file1.dat");
            storage.OpenFileReadWrite("file2.dat");
            storage.OpenFileReadWrite("file3.dat");

            auto result = storage.GetAllFileNames();
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value().size(), 3U);
        }

        TEST_F(FileStorageTest, OpenNonExistentFileReadOnly)
        {
            FileStorage storage(cTestDir);

            auto result = storage.OpenFileReadOnly("nonexistent.dat");
            EXPECT_FALSE(result.HasValue());
        }

        TEST_F(FileStorageTest, GetFileSize)
        {
            FileStorage storage(cTestDir);

            {
                auto result = storage.OpenFileReadWrite("sized.dat");
                ASSERT_TRUE(result.HasValue());
                const std::uint8_t data[] = {1, 2, 3, 4, 5};
                result.Value()->Write(data, sizeof(data));
                result.Value()->Sync();
            }

            {
                auto result = storage.OpenFileReadOnly("sized.dat");
                ASSERT_TRUE(result.HasValue());
                auto sizeResult = result.Value()->GetSize();
                ASSERT_TRUE(sizeResult.HasValue());
                EXPECT_EQ(sizeResult.Value(), 5U);
            }
        }
    }
}
