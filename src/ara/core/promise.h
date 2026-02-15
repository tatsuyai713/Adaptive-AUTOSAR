#ifndef PROMISE_H
#define PROMISE_H

#include <future>
#include "./result.h"
#include "./future.h"

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR AP Promise type that provides a facility to store a Result
        ///        that is later acquired asynchronously via a Future
        /// @tparam T Value type
        /// @tparam E Error type
        template <typename T, typename E = ErrorCode>
        class Promise final
        {
        private:
            std::promise<Result<T, E>> mPromise;

        public:
            Promise() = default;
            ~Promise() = default;

            Promise(const Promise &) = delete;
            Promise &operator=(const Promise &) = delete;

            Promise(Promise &&other) noexcept
                : mPromise{std::move(other.mPromise)}
            {
            }

            Promise &operator=(Promise &&other) noexcept
            {
                if (this != &other)
                {
                    mPromise = std::move(other.mPromise);
                }
                return *this;
            }

            /// @brief Get the Future associated with this Promise
            /// @returns A Future that shares state with this Promise
            Future<T, E> get_future()
            {
                return Future<T, E>(mPromise.get_future());
            }

            /// @brief Set a Result as the shared state
            /// @param result The Result to store
            void SetResult(const Result<T, E> &result)
            {
                mPromise.set_value(result);
            }

            /// @brief Move a Result into the shared state
            /// @param result The Result to store
            void SetResult(Result<T, E> &&result)
            {
                mPromise.set_value(std::move(result));
            }

            /// @brief Set a value as the shared state
            /// @param value The value to store
            void set_value(const T &value)
            {
                mPromise.set_value(Result<T, E>::FromValue(value));
            }

            /// @brief Move a value into the shared state
            /// @param value The value to store
            void set_value(T &&value)
            {
                mPromise.set_value(Result<T, E>::FromValue(std::move(value)));
            }

            /// @brief Set an error as the shared state
            /// @param error The error to store
            void SetError(const E &error)
            {
                mPromise.set_value(Result<T, E>::FromError(error));
            }

            /// @brief Move an error into the shared state
            /// @param error The error to store
            void SetError(E &&error)
            {
                mPromise.set_value(Result<T, E>::FromError(std::move(error)));
            }
        };

        /// @brief Specialization of Promise for void value type
        /// @tparam E Error type
        template <typename E>
        class Promise<void, E> final
        {
        private:
            std::promise<Result<void, E>> mPromise;

        public:
            Promise() = default;
            ~Promise() = default;

            Promise(const Promise &) = delete;
            Promise &operator=(const Promise &) = delete;

            Promise(Promise &&other) noexcept
                : mPromise{std::move(other.mPromise)}
            {
            }

            Promise &operator=(Promise &&other) noexcept
            {
                if (this != &other)
                {
                    mPromise = std::move(other.mPromise);
                }
                return *this;
            }

            /// @brief Get the Future associated with this Promise
            /// @returns A Future that shares state with this Promise
            Future<void, E> get_future()
            {
                return Future<void, E>(mPromise.get_future());
            }

            /// @brief Set a void Result as the shared state
            void set_value()
            {
                mPromise.set_value(Result<void, E>::FromValue());
            }

            /// @brief Set a Result as the shared state
            /// @param result The Result to store
            void SetResult(const Result<void, E> &result)
            {
                mPromise.set_value(result);
            }

            /// @brief Move a Result into the shared state
            /// @param result The Result to store
            void SetResult(Result<void, E> &&result)
            {
                mPromise.set_value(std::move(result));
            }

            /// @brief Set an error as the shared state
            /// @param error The error to store
            void SetError(const E &error)
            {
                mPromise.set_value(Result<void, E>::FromError(error));
            }

            /// @brief Move an error into the shared state
            /// @param error The error to store
            void SetError(E &&error)
            {
                mPromise.set_value(Result<void, E>::FromError(std::move(error)));
            }
        };
    }
}

#endif
