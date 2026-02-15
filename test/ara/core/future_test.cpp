#include <gtest/gtest.h>
#include <chrono>
#include "../../../src/ara/core/future.h"
#include "../../../src/ara/core/promise.h"

namespace ara
{
    namespace core
    {
        TEST(FutureTest, DefaultConstructor)
        {
            Future<int> _future;
            EXPECT_FALSE(_future.valid());
        }

        TEST(FutureTest, MoveConstructor)
        {
            Promise<int> _promise;
            Future<int> _future = _promise.get_future();
            EXPECT_TRUE(_future.valid());

            Future<int> _movedFuture{std::move(_future)};
            EXPECT_TRUE(_movedFuture.valid());
            EXPECT_FALSE(_future.valid());
        }

        TEST(FutureTest, MoveAssignment)
        {
            Promise<int> _promise;
            Future<int> _future = _promise.get_future();

            Future<int> _other;
            _other = std::move(_future);
            EXPECT_TRUE(_other.valid());
            EXPECT_FALSE(_future.valid());
        }

        TEST(FutureTest, GetResultWithValue)
        {
            Promise<int> _promise;
            Future<int> _future = _promise.get_future();

            const int cExpected{42};
            _promise.set_value(cExpected);

            Result<int> _result = _future.GetResult();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(cExpected, _result.Value());
        }

        TEST(FutureTest, GetResultWithError)
        {
            const ErrorDomain::IdType cDomainId{0x8000000000000201};

            class TestErrorDomain final : public ErrorDomain
            {
            public:
                TestErrorDomain(IdType id) : ErrorDomain{id} {}
                const char *Name() const noexcept override { return "Test"; }
                const char *Message(CodeType) const noexcept override { return "test error"; }
            };

            TestErrorDomain _domain{cDomainId};
            ErrorCode _error{1, _domain};

            Promise<int> _promise;
            Future<int> _future = _promise.get_future();

            _promise.SetError(_error);

            Result<int> _result = _future.GetResult();
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(FutureTest, WaitFor)
        {
            Promise<int> _promise;
            Future<int> _future = _promise.get_future();

            // Should timeout since no value is set yet
            auto _status = _future.wait_for(std::chrono::milliseconds(10));
            EXPECT_EQ(future_status::timeout, _status);

            // Set value and wait again
            _promise.set_value(1);
            _status = _future.wait_for(std::chrono::milliseconds(100));
            EXPECT_EQ(future_status::ready, _status);
        }

        TEST(FutureVoidTest, GetResultVoid)
        {
            Promise<void> _promise;
            Future<void> _future = _promise.get_future();

            _promise.set_value();

            Result<void> _result = _future.GetResult();
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(FutureVoidTest, MoveConstructor)
        {
            Promise<void> _promise;
            Future<void> _future = _promise.get_future();

            Future<void> _movedFuture{std::move(_future)};
            EXPECT_TRUE(_movedFuture.valid());
            EXPECT_FALSE(_future.valid());
        }

        TEST(FutureTest, ThenWithValueContinuation)
        {
            Promise<int> _promise;
            Future<int> _future = _promise.get_future();

            auto _nextFuture = _future.then(
                [](Future<int> readyFuture)
                {
                    auto _result = readyFuture.GetResult();
                    return _result.Value() * 2;
                });

            _promise.set_value(21);

            auto _result = _nextFuture.GetResult();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(42, _result.Value());
        }

        TEST(FutureVoidTest, ThenWithVoidInputProducesValue)
        {
            Promise<void> _promise;
            Future<void> _future = _promise.get_future();

            auto _nextFuture = _future.then(
                [](Future<void> readyFuture)
                {
                    auto _result = readyFuture.GetResult();
                    if (_result.HasValue())
                    {
                        return 7;
                    }

                    return 0;
                });

            _promise.set_value();

            auto _result = _nextFuture.GetResult();
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(7, _result.Value());
        }
    }
}
