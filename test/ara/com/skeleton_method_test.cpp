#include <gtest/gtest.h>
#include "../../../src/ara/com/method.h"
#include "../../../src/ara/com/serialization.h"
#include "./mock_event_binding.h"

namespace ara
{
    namespace com
    {
        // ── Helper: build a flat serialized arg buffer ──

        template <typename T>
        std::vector<std::uint8_t> MakeRequest(const T &value)
        {
            return Serializer<T>::Serialize(value);
        }

        template <typename T1, typename T2>
        std::vector<std::uint8_t> MakeRequest(const T1 &v1, const T2 &v2)
        {
            auto b1 = Serializer<T1>::Serialize(v1);
            auto b2 = Serializer<T2>::Serialize(v2);
            b1.insert(b1.end(), b2.begin(), b2.end());
            return b1;
        }

        template <typename T1, typename T2, typename T3>
        std::vector<std::uint8_t> MakeRequest(const T1 &v1, const T2 &v2, const T3 &v3)
        {
            auto b = MakeRequest(v1, v2);
            auto b3 = Serializer<T3>::Serialize(v3);
            b.insert(b.end(), b3.begin(), b3.end());
            return b;
        }

        // ── SkeletonMethod<R(Args...)> Tests ──

        TEST(SkeletonMethodTest, SetHandlerAndInvokeWithReturnValue)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<int(int, int)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const int &a, const int &b) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(a + b);
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());
            EXPECT_TRUE(raw->IsRegistered());

            // Simulate incoming request: 3 + 5 = 8
            auto request = MakeRequest(3, 5);
            auto response = raw->InvokeRequest(request);

            ASSERT_TRUE(response.HasValue());
            auto result = Serializer<int>::Deserialize(
                response.Value().data(), response.Value().size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 8);
        }

        TEST(SkeletonMethodTest, SetHandlerNoArgs)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<float()> method{std::move(binding)};

            auto regResult = method.SetHandler(
                []() -> core::Future<float>
                {
                    core::Promise<float> p;
                    p.set_value(3.14f);
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto response = raw->InvokeRequest({});
            ASSERT_TRUE(response.HasValue());
            auto result = Serializer<float>::Deserialize(
                response.Value().data(), response.Value().size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_FLOAT_EQ(result.Value(), 3.14f);
        }

        TEST(SkeletonMethodTest, SetHandlerSingleArg)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<double(double)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const double &x) -> core::Future<double>
                {
                    core::Promise<double> p;
                    p.set_value(x * 2.0);
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto request = MakeRequest(5.0);
            auto response = raw->InvokeRequest(request);

            ASSERT_TRUE(response.HasValue());
            auto result = Serializer<double>::Deserialize(
                response.Value().data(), response.Value().size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_DOUBLE_EQ(result.Value(), 10.0);
        }

        TEST(SkeletonMethodTest, SetHandlerThreeArgs)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<int(int, int, int)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const int &a, const int &b, const int &c) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(a + b + c);
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto request = MakeRequest(1, 2, 3);
            auto response = raw->InvokeRequest(request);

            ASSERT_TRUE(response.HasValue());
            auto result = Serializer<int>::Deserialize(
                response.Value().data(), response.Value().size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), 6);
        }

        TEST(SkeletonMethodTest, VoidReturnWithArg)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            std::uint32_t captured = 0U;
            SkeletonMethod<void(std::uint32_t)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [&captured](const std::uint32_t &v) -> core::Future<void>
                {
                    captured = v;
                    core::Promise<void> p;
                    p.set_value();
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto request = MakeRequest<std::uint32_t>(42U);
            auto response = raw->InvokeRequest(request);

            ASSERT_TRUE(response.HasValue());
            EXPECT_TRUE(response.Value().empty());
            EXPECT_EQ(captured, 42U);
        }

        TEST(SkeletonMethodTest, VoidReturnNoArgs)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            bool called = false;
            SkeletonMethod<void()> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [&called]() -> core::Future<void>
                {
                    called = true;
                    core::Promise<void> p;
                    p.set_value();
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto response = raw->InvokeRequest({});
            ASSERT_TRUE(response.HasValue());
            EXPECT_TRUE(called);
        }

        TEST(SkeletonMethodTest, HandlerReturnsError)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<int(int)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const int &) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.SetError(MakeErrorCode(ComErrc::kServiceNotAvailable));
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto request = MakeRequest(0);
            auto response = raw->InvokeRequest(request);
            EXPECT_FALSE(response.HasValue());
        }

        TEST(SkeletonMethodTest, NullBindingSetHandlerFails)
        {
            SkeletonMethod<int(int)> method{nullptr};

            auto regResult = method.SetHandler(
                [](const int &v) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(v);
                    return p.get_future();
                });

            EXPECT_FALSE(regResult.HasValue());
        }

        TEST(SkeletonMethodTest, BindingFailsRegistration)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            binding->ShouldFailRegistration = true;

            SkeletonMethod<int(int)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const int &v) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(v);
                    return p.get_future();
                });

            EXPECT_FALSE(regResult.HasValue());
        }

        TEST(SkeletonMethodTest, UnsetHandlerDeregisters)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<int(int)> method{std::move(binding)};

            method.SetHandler(
                [](const int &v) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(v);
                    return p.get_future();
                });

            EXPECT_TRUE(raw->IsRegistered());

            method.UnsetHandler();

            EXPECT_FALSE(raw->IsRegistered());
        }

        TEST(SkeletonMethodTest, MalformedRequestReturnsError)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<int(int)> method{std::move(binding)};

            method.SetHandler(
                [](const int &v) -> core::Future<int>
                {
                    core::Promise<int> p;
                    p.set_value(v);
                    return p.get_future();
                });

            // Empty request when int is expected — should fail deserialization
            auto response = raw->InvokeRequest({});
            EXPECT_FALSE(response.HasValue());
        }

        TEST(SkeletonMethodTest, StringArgAndReturn)
        {
            auto binding = std::make_unique<test::MockSkeletonMethodBinding>();
            auto *raw = binding.get();

            SkeletonMethod<std::string(std::string)> method{std::move(binding)};

            auto regResult = method.SetHandler(
                [](const std::string &s) -> core::Future<std::string>
                {
                    core::Promise<std::string> p;
                    p.set_value("echo:" + s);
                    return p.get_future();
                });

            ASSERT_TRUE(regResult.HasValue());

            auto request = Serializer<std::string>::Serialize("hello");
            auto response = raw->InvokeRequest(request);

            ASSERT_TRUE(response.HasValue());
            auto result = Serializer<std::string>::Deserialize(
                response.Value().data(), response.Value().size());
            ASSERT_TRUE(result.HasValue());
            EXPECT_EQ(result.Value(), "echo:hello");
        }
    }
}
