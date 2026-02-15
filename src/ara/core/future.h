/// @file src/ara/core/future.h
/// @brief Declarations for future.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef FUTURE_H
#define FUTURE_H

#include <future>
#include <chrono>
#include <type_traits>
#include "./result.h"
#include "./error_code.h"

namespace ara
{
    namespace core
    {
        /// @brief Specifies the state of a Future as returned by wait_for and wait_until
        enum class future_status : uint8_t
        {
            ready = 0,   ///< The shared state is ready
            timeout = 1  ///< The shared state did not become ready before the specified timeout has passed
        };

        /// @brief Forward declaration of `Future`.
        template <typename T, typename E>
        class Future;

        /// @brief Forward declaration of `Promise`.
        template <typename T, typename E>
        class Promise;

        /// @brief AUTOSAR AP Future type providing a mechanism to access the result of asynchronous operations
        /// @tparam T Value type
        /// @tparam E Error type
        template <typename T, typename E = ErrorCode>
        class Future final
        {
            template <typename U, typename G>
            friend class Future;
            friend class Promise<T, E>;

        private:
            std::future<Result<T, E>> mFuture;

            template <typename R, typename FCallable>
            static typename std::enable_if<!std::is_void<R>::value, Result<R, E>>::type
            InvokeContinuation(FCallable &func, Future<T, E> &&readyFuture)
            {
                return Result<R, E>::FromValue(func(std::move(readyFuture)));
            }

            template <typename R, typename FCallable>
            static typename std::enable_if<std::is_void<R>::value, Result<R, E>>::type
            InvokeContinuation(FCallable &func, Future<T, E> &&readyFuture)
            {
                func(std::move(readyFuture));
                return Result<void, E>::FromValue();
            }

            Future(std::future<Result<T, E>> &&future) noexcept
                : mFuture{std::move(future)}
            {
            }

        public:
            /// @brief Constructs an empty future.
            Future() noexcept = default;
            /// @brief Destructor.
            ~Future() noexcept = default;

            Future(const Future &) = delete;
            Future &operator=(const Future &) = delete;

            /// @brief Move constructor.
            /// @param other Source future.
            Future(Future &&other) noexcept
                : mFuture{std::move(other.mFuture)}
            {
            }

            /// @brief Move assignment.
            /// @param other Source future.
            /// @returns Reference to `*this`.
            Future &operator=(Future &&other) noexcept
            {
                if (this != &other)
                {
                    mFuture = std::move(other.mFuture);
                }
                return *this;
            }

            /// @brief Get the result, blocking until it becomes available
            /// @returns Result containing either the value or an error
            Result<T, E> GetResult()
            {
                return mFuture.get();
            }

            /// @brief Check if the Future has a valid shared state
            /// @returns True if the Future is valid
            bool valid() const noexcept
            {
                return mFuture.valid();
            }

            /// @brief Check whether the shared state is already ready without blocking.
            /// @returns True if result is ready; otherwise false.
            bool is_ready() const
            {
                if (!mFuture.valid())
                {
                    return false;
                }

                const auto cStatus{mFuture.wait_for(std::chrono::milliseconds(0))};
                return cStatus == std::future_status::ready;
            }

            /// @brief Block until the shared state is ready
            void wait() const
            {
                mFuture.wait();
            }

            /// @brief Wait for a specified duration
            /// @tparam Rep Duration arithmetic type
            /// @tparam Period Duration period type
            /// @param timeoutDuration Maximum duration to wait
            /// @returns Status of the shared state
            template <typename Rep, typename Period>
            future_status wait_for(
                const std::chrono::duration<Rep, Period> &timeoutDuration) const
            {
                auto _status = mFuture.wait_for(timeoutDuration);
                return _status == std::future_status::ready
                           ? future_status::ready
                           : future_status::timeout;
            }

            /// @brief Wait until a specified time point
            /// @tparam Clock Clock type
            /// @tparam Duration Duration type
            /// @param deadline Time point to wait until
            /// @returns Status of the shared state
            template <typename Clock, typename Duration>
            future_status wait_until(
                const std::chrono::time_point<Clock, Duration> &deadline) const
            {
                auto _status = mFuture.wait_until(deadline);
                return _status == std::future_status::ready
                           ? future_status::ready
                           : future_status::timeout;
            }

            /// @brief Apply a continuation to the Future
            /// @tparam F Callable type that takes a Future<T,E> and returns a value
            /// @param func Callable to apply when the Future becomes ready
            /// @returns A new Future containing the result of the continuation
            template <typename F>
            auto then(F &&func) -> Future<decltype(func(std::move(*this))), E>
            {
                using ResultType = decltype(func(std::move(*this)));

                auto _sharedFuture =
                    std::make_shared<std::future<Result<T, E>>>(std::move(mFuture));
                auto _task =
                    std::async(std::launch::async,
                               [_sharedFuture, func = std::forward<F>(func)]() mutable
                               {
                                   _sharedFuture->wait();
                                   Future<T, E> _readyFuture(std::move(*_sharedFuture));
                                   return InvokeContinuation<ResultType>(
                                       func,
                                       std::move(_readyFuture));
                               });

                return Future<ResultType, E>(std::move(_task));
            }
        };

        /// @brief Specialization of Future for void value type
        /// @tparam E Error type
        template <typename E>
        class Future<void, E> final
        {
            template <typename U, typename G>
            friend class Future;
            friend class Promise<void, E>;

        private:
            std::future<Result<void, E>> mFuture;

            template <typename R, typename FCallable>
            static typename std::enable_if<!std::is_void<R>::value, Result<R, E>>::type
            InvokeContinuation(FCallable &func, Future<void, E> &&readyFuture)
            {
                return Result<R, E>::FromValue(func(std::move(readyFuture)));
            }

            template <typename R, typename FCallable>
            static typename std::enable_if<std::is_void<R>::value, Result<R, E>>::type
            InvokeContinuation(FCallable &func, Future<void, E> &&readyFuture)
            {
                func(std::move(readyFuture));
                return Result<void, E>::FromValue();
            }

            Future(std::future<Result<void, E>> &&future) noexcept
                : mFuture{std::move(future)}
            {
            }

        public:
            /// @brief Constructs an empty future.
            Future() noexcept = default;
            /// @brief Destructor.
            ~Future() noexcept = default;

            Future(const Future &) = delete;
            Future &operator=(const Future &) = delete;

            /// @brief Move constructor.
            /// @param other Source future.
            Future(Future &&other) noexcept
                : mFuture{std::move(other.mFuture)}
            {
            }

            /// @brief Move assignment.
            /// @param other Source future.
            /// @returns Reference to `*this`.
            Future &operator=(Future &&other) noexcept
            {
                if (this != &other)
                {
                    mFuture = std::move(other.mFuture);
                }
                return *this;
            }

            /// @brief Get the result, blocking until it becomes available
            /// @returns Result containing either void success or an error
            Result<void, E> GetResult()
            {
                return mFuture.get();
            }

            /// @brief Check if the Future has a valid shared state
            /// @returns True if the Future is valid
            bool valid() const noexcept
            {
                return mFuture.valid();
            }

            /// @brief Check whether the shared state is already ready without blocking.
            /// @returns True if result is ready; otherwise false.
            bool is_ready() const
            {
                if (!mFuture.valid())
                {
                    return false;
                }

                const auto cStatus{mFuture.wait_for(std::chrono::milliseconds(0))};
                return cStatus == std::future_status::ready;
            }

            /// @brief Block until the shared state is ready
            void wait() const
            {
                mFuture.wait();
            }

            /// @brief Wait for a specified duration
            template <typename Rep, typename Period>
            future_status wait_for(
                const std::chrono::duration<Rep, Period> &timeoutDuration) const
            {
                auto _status = mFuture.wait_for(timeoutDuration);
                return _status == std::future_status::ready
                           ? future_status::ready
                           : future_status::timeout;
            }

            /// @brief Wait until a specified time point
            template <typename Clock, typename Duration>
            future_status wait_until(
                const std::chrono::time_point<Clock, Duration> &deadline) const
            {
                auto _status = mFuture.wait_until(deadline);
                return _status == std::future_status::ready
                           ? future_status::ready
                           : future_status::timeout;
            }

            /// @brief Apply a continuation to the Future<void, E>
            /// @tparam F Callable type that takes a Future<void,E>
            /// @param func Callable to apply when the Future becomes ready
            /// @returns A new Future containing the continuation result
            template <typename F>
            auto then(F &&func) -> Future<decltype(func(std::move(*this))), E>
            {
                using ResultType = decltype(func(std::move(*this)));

                auto _sharedFuture =
                    std::make_shared<std::future<Result<void, E>>>(std::move(mFuture));
                auto _task =
                    std::async(std::launch::async,
                               [_sharedFuture, func = std::forward<F>(func)]() mutable
                               {
                                   _sharedFuture->wait();
                                   Future<void, E> _readyFuture(std::move(*_sharedFuture));
                                   return InvokeContinuation<ResultType>(
                                       func,
                                       std::move(_readyFuture));
                               });

                return Future<ResultType, E>(std::move(_task));
            }
        };
    }
}

#endif
