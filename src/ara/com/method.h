/// @file src/ara/com/method.h
/// @brief Declarations for method.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_METHOD_H
#define ARA_COM_METHOD_H

#include <memory>
#include <utility>
#include <vector>
#include "./serialization.h"
#include "./internal/method_binding.h"
#include "../core/future.h"
#include "../core/promise.h"
#include "../core/result.h"

namespace ara
{
    namespace com
    {
        /// @brief Primary template declaration for ProxyMethod.
        ///        Usage: ProxyMethod<ReturnType(Arg1, Arg2, ...)>
        template <typename Signature>
        class ProxyMethod;

        /// @brief Proxy-side method wrapper per AUTOSAR AP.
        ///        Provides operator() that serializes arguments, sends via binding,
        ///        deserializes the response, and returns it as a Future<R>.
        /// @tparam R Return type
        /// @tparam Args Argument types
        template <typename R, typename... Args>
        class ProxyMethod<R(Args...)>
        {
        private:
            std::unique_ptr<internal::ProxyMethodBinding> mBinding;

            static void AppendSerialized(std::vector<std::uint8_t> &) {}

            template <typename First, typename... Rest>
            static void AppendSerialized(
                std::vector<std::uint8_t> &payload,
                const First &first,
                const Rest &...rest)
            {
                auto bytes = Serializer<First>::Serialize(first);
                payload.insert(payload.end(), bytes.begin(), bytes.end());
                AppendSerialized(payload, rest...);
            }

            static std::vector<std::uint8_t> SerializeArgs(const Args &...args)
            {
                std::vector<std::uint8_t> payload;
                AppendSerialized(payload, args...);
                return payload;
            }

        public:
            /// @brief Creates a proxy method wrapper bound to transport implementation.
            /// @param binding Concrete method binding implementation.
            explicit ProxyMethod(
                std::unique_ptr<internal::ProxyMethodBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            ProxyMethod(const ProxyMethod &) = delete;
            ProxyMethod &operator=(const ProxyMethod &) = delete;
            ProxyMethod(ProxyMethod &&) noexcept = default;
            ProxyMethod &operator=(ProxyMethod &&) noexcept = default;

            /// @brief Invoke the remote method
            /// @param args Method arguments
            /// @returns Future containing the result
            core::Future<R> operator()(Args... args)
            {
                core::Promise<R> promise;
                auto future = promise.get_future();

                if (!mBinding)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                    return future;
                }

                auto serializedArgs = SerializeArgs(args...);

                mBinding->Call(
                    serializedArgs,
                    [p = std::make_shared<core::Promise<R>>(std::move(promise))](
                        core::Result<std::vector<std::uint8_t>> rawResult) mutable
                    {
                        if (!rawResult.HasValue())
                        {
                            p->SetError(rawResult.Error());
                            return;
                        }

                        const auto &responseBytes = rawResult.Value();
                        auto deserResult = Serializer<R>::Deserialize(
                            responseBytes.data(),
                            responseBytes.size());

                        if (deserResult.HasValue())
                        {
                            p->set_value(std::move(deserResult).Value());
                        }
                        else
                        {
                            p->SetError(deserResult.Error());
                        }
                    });

                return future;
            }
        };

        /// @brief Specialization for void return type (fire-and-forget or ack)
        template <typename... Args>
        class ProxyMethod<void(Args...)>
        {
        private:
            std::unique_ptr<internal::ProxyMethodBinding> mBinding;

            static void AppendSerialized(std::vector<std::uint8_t> &) {}

            template <typename First, typename... Rest>
            static void AppendSerialized(
                std::vector<std::uint8_t> &payload,
                const First &first,
                const Rest &...rest)
            {
                auto bytes = Serializer<First>::Serialize(first);
                payload.insert(payload.end(), bytes.begin(), bytes.end());
                AppendSerialized(payload, rest...);
            }

            static std::vector<std::uint8_t> SerializeArgs(const Args &...args)
            {
                std::vector<std::uint8_t> payload;
                AppendSerialized(payload, args...);
                return payload;
            }

        public:
            /// @brief Creates a void-return proxy method wrapper.
            /// @param binding Concrete method binding implementation.
            explicit ProxyMethod(
                std::unique_ptr<internal::ProxyMethodBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            ProxyMethod(const ProxyMethod &) = delete;
            ProxyMethod &operator=(const ProxyMethod &) = delete;
            ProxyMethod(ProxyMethod &&) noexcept = default;
            ProxyMethod &operator=(ProxyMethod &&) noexcept = default;

            /// @brief Invokes a remote method that has no return payload.
            /// @param args Method arguments.
            /// @returns Future that resolves on acknowledgement or error.
            core::Future<void> operator()(Args... args)
            {
                core::Promise<void> promise;
                auto future = promise.get_future();

                if (!mBinding)
                {
                    promise.SetError(
                        MakeErrorCode(ComErrc::kServiceNotAvailable));
                    return future;
                }

                auto serializedArgs = SerializeArgs(args...);

                mBinding->Call(
                    serializedArgs,
                    [p = std::make_shared<core::Promise<void>>(std::move(promise))](
                        core::Result<std::vector<std::uint8_t>> rawResult) mutable
                    {
                        if (!rawResult.HasValue())
                        {
                            p->SetError(rawResult.Error());
                            return;
                        }

                        p->set_value();
                    });

                return future;
            }
        };
    }
}

#endif
