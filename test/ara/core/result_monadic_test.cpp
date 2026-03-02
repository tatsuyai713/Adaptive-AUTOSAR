#include <gtest/gtest.h>
#include "../../../src/ara/core/result.h"

namespace ara
{
    namespace core
    {
        // ------------------------------------------------------------------
        // Result<T,E> — AndThen
        // ------------------------------------------------------------------

        TEST(ResultMonadicTest, AndThenCallsCallableOnValue)
        {
            Result<int, int> _r{42};
            const auto _out{_r.AndThen([](int v) -> Result<std::string, int>
                                       { return Result<std::string, int>{std::to_string(v)}; })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), "42");
        }

        TEST(ResultMonadicTest, AndThenPropagatesError)
        {
            Result<int, int> _r{Result<int, int>::FromError(99)};
            const auto _out{_r.AndThen([](int v) -> Result<std::string, int>
                                       { return Result<std::string, int>{std::to_string(v)}; })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), 99);
        }

        // ------------------------------------------------------------------
        // Result<T,E> — OrElse
        // ------------------------------------------------------------------

        TEST(ResultMonadicTest, OrElsePropagatesValueUnchanged)
        {
            Result<int, int> _r{7};
            const auto _out{_r.OrElse([](int) -> Result<int, int>
                                      { return Result<int, int>{0}; })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 7);
        }

        TEST(ResultMonadicTest, OrElseCallsCallableOnError)
        {
            Result<int, int> _r{Result<int, int>::FromError(3)};
            const auto _out{_r.OrElse([](int e) -> Result<int, int>
                                      { return Result<int, int>{e * 10}; })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 30);
        }

        // ------------------------------------------------------------------
        // Result<T,E> — Map
        // ------------------------------------------------------------------

        TEST(ResultMonadicTest, MapTransformsValue)
        {
            Result<int, int> _r{5};
            const auto _out{_r.Map([](int v) { return v * 2; })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 10);
        }

        TEST(ResultMonadicTest, MapPropagatesErrorUnchanged)
        {
            Result<int, int> _r{Result<int, int>::FromError(42)};
            const auto _out{_r.Map([](int v) { return v * 2; })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), 42);
        }

        // ------------------------------------------------------------------
        // Result<T,E> — MapError
        // ------------------------------------------------------------------

        TEST(ResultMonadicTest, MapErrorTransformsError)
        {
            Result<int, int> _r{Result<int, int>::FromError(3)};
            const auto _out{_r.MapError([](int e) { return std::to_string(e); })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), "3");
        }

        TEST(ResultMonadicTest, MapErrorPropagatesValueUnchanged)
        {
            Result<int, int> _r{10};
            const auto _out{_r.MapError([](int e) { return std::to_string(e); })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 10);
        }

        // ------------------------------------------------------------------
        // Result<void,E> — AndThen
        // ------------------------------------------------------------------

        TEST(ResultVoidMonadicTest, AndThenCallsCallableOnOk)
        {
            Result<void, int> _r;
            bool _called{false};
            const auto _out{_r.AndThen([&]() -> Result<int, int>
                                       {
                                           _called = true;
                                           return Result<int, int>{1};
                                       })};
            ASSERT_TRUE(_called);
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 1);
        }

        TEST(ResultVoidMonadicTest, AndThenPropagatesError)
        {
            Result<void, int> _r{Result<void, int>::FromError(7)};
            const auto _out{_r.AndThen([&]() -> Result<int, int>
                                       { return Result<int, int>{1}; })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), 7);
        }

        // ------------------------------------------------------------------
        // Result<void,E> — OrElse
        // ------------------------------------------------------------------

        TEST(ResultVoidMonadicTest, OrElsePropagatesOk)
        {
            Result<void, int> _r;
            bool _called{false};
            const auto _out{_r.OrElse([&](int) -> Result<void, int>
                                      {
                                          _called = true;
                                          return Result<void, int>::FromError(99);
                                      })};
            EXPECT_FALSE(_called);
            EXPECT_TRUE(_out.HasValue());
        }

        TEST(ResultVoidMonadicTest, OrElseCallsCallableOnError)
        {
            Result<void, int> _r{Result<void, int>::FromError(5)};
            bool _called{false};
            const auto _out{_r.OrElse([&](int) -> Result<void, int>
                                      {
                                          _called = true;
                                          return Result<void, int>::FromValue();
                                      })};
            EXPECT_TRUE(_called);
            EXPECT_TRUE(_out.HasValue());
        }

        // ------------------------------------------------------------------
        // Result<void,E> — Map
        // ------------------------------------------------------------------

        TEST(ResultVoidMonadicTest, MapProducesValueWhenOk)
        {
            Result<void, int> _r;
            const auto _out{_r.Map([]() { return 42; })};
            ASSERT_TRUE(_out.HasValue());
            EXPECT_EQ(_out.Value(), 42);
        }

        TEST(ResultVoidMonadicTest, MapPropagatesError)
        {
            Result<void, int> _r{Result<void, int>::FromError(99)};
            const auto _out{_r.Map([]() { return 42; })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), 99);
        }

        // ------------------------------------------------------------------
        // Result<void,E> — MapError
        // ------------------------------------------------------------------

        TEST(ResultVoidMonadicTest, MapErrorTransformsError)
        {
            Result<void, int> _r{Result<void, int>::FromError(3)};
            const auto _out{_r.MapError([](int e) { return std::to_string(e); })};
            ASSERT_FALSE(_out.HasValue());
            EXPECT_EQ(_out.Error(), "3");
        }

        TEST(ResultVoidMonadicTest, MapErrorPreservesOk)
        {
            Result<void, int> _r;
            const auto _out{_r.MapError([](int e) { return std::to_string(e); })};
            EXPECT_TRUE(_out.HasValue());
        }

    } // namespace core
} // namespace ara
