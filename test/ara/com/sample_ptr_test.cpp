#include <gtest/gtest.h>
#include "../../../src/ara/com/sample_ptr.h"

namespace ara
{
    namespace com
    {
        TEST(SamplePtrTest, ConstructAndDereference)
        {
            SamplePtr<int> sample{std::make_unique<const int>(42)};
            ASSERT_NE(sample, nullptr);
            EXPECT_EQ(*sample, 42);
        }

        TEST(SamplePtrTest, MoveSemantics)
        {
            SamplePtr<int> a{std::make_unique<const int>(10)};
            SamplePtr<int> b{std::move(a)};

            EXPECT_EQ(a, nullptr);
            ASSERT_NE(b, nullptr);
            EXPECT_EQ(*b, 10);
        }

        TEST(SamplePtrTest, NullByDefault)
        {
            SamplePtr<int> sample;
            EXPECT_EQ(sample, nullptr);
        }

        struct TestPayload
        {
            std::uint32_t Id;
            double Value;
        };

        TEST(SamplePtrTest, StructAccess)
        {
            auto payload = std::make_unique<const TestPayload>(
                TestPayload{100U, 3.14});
            SamplePtr<TestPayload> sample{std::move(payload)};

            EXPECT_EQ(sample->Id, 100U);
            EXPECT_DOUBLE_EQ(sample->Value, 3.14);
        }

        TEST(SampleAllocateePtrTest, ConstructAndAccess)
        {
            auto raw = new int{55};
            auto deleter = [](int *ptr) { delete ptr; };
            SampleAllocateePtr<int> ptr{
                std::unique_ptr<int, std::function<void(int *)>>(raw, deleter)};

            ASSERT_TRUE(static_cast<bool>(ptr));
            EXPECT_EQ(*ptr, 55);
        }

        TEST(SampleAllocateePtrTest, MoveSemantics)
        {
            auto raw = new int{77};
            auto deleter = [](int *ptr) { delete ptr; };
            SampleAllocateePtr<int> a{
                std::unique_ptr<int, std::function<void(int *)>>(raw, deleter)};
            SampleAllocateePtr<int> b{std::move(a)};

            EXPECT_FALSE(static_cast<bool>(a));
            ASSERT_TRUE(static_cast<bool>(b));
            EXPECT_EQ(*b, 77);
        }

        TEST(SampleAllocateePtrTest, ModifyInPlace)
        {
            auto raw = new TestPayload{0U, 0.0};
            auto deleter = [](TestPayload *ptr) { delete ptr; };
            SampleAllocateePtr<TestPayload> ptr{
                std::unique_ptr<TestPayload, std::function<void(TestPayload *)>>(
                    raw, deleter)};

            ptr->Id = 42U;
            ptr->Value = 2.718;

            EXPECT_EQ(ptr->Id, 42U);
            EXPECT_DOUBLE_EQ(ptr->Value, 2.718);
        }

        TEST(SampleAllocateePtrTest, Release)
        {
            auto raw = new int{99};
            auto deleter = [](int *ptr) { delete ptr; };
            SampleAllocateePtr<int> ptr{
                std::unique_ptr<int, std::function<void(int *)>>(raw, deleter)};

            int *released = ptr.Release();
            EXPECT_FALSE(static_cast<bool>(ptr));
            EXPECT_EQ(*released, 99);
            delete released;
        }

        TEST(SampleAllocateePtrTest, DefaultIsNull)
        {
            SampleAllocateePtr<int> ptr;
            EXPECT_FALSE(static_cast<bool>(ptr));
        }

        TEST(SampleAllocateePtrTest, Swap)
        {
            auto rawA = new int{1};
            auto rawB = new int{2};
            auto deleter = [](int *ptr) { delete ptr; };

            SampleAllocateePtr<int> a{
                std::unique_ptr<int, std::function<void(int *)>>(rawA, deleter)};
            SampleAllocateePtr<int> b{
                std::unique_ptr<int, std::function<void(int *)>>(rawB, deleter)};

            a.Swap(b);

            EXPECT_EQ(*a, 2);
            EXPECT_EQ(*b, 1);
        }
    }
}
