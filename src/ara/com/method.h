/// @file src/ara/com/method.h
/// @brief Declarations for method.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_METHOD_H
#define ARA_COM_METHOD_H

#include <memory>
#include <mutex>
#include <tuple>
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
        namespace detail
        {
            /// @brief Sequentially deserializes a tuple of typed arguments from a flat byte buffer.
            ///        Used internally by SkeletonMethod to unpack multi-arg requests.
            template <typename... Args>
            struct TupleDeserializer;

            /// @brief Base case: no args, nothing to deserialize.
            template <>
            struct TupleDeserializer<>
            {
                static core::Result<std::tuple<>> Deserialize(
                    const std::vector<std::uint8_t> &,
                    std::size_t &)
                {
                    return core::Result<std::tuple<>>::FromValue(std::make_tuple());
                }
            };

            /// @brief Recursive case: deserialize First, then Rest.
            template <typename First, typename... Rest>
            struct TupleDeserializer<First, Rest...>
            {
                static core::Result<std::tuple<First, Rest...>> Deserialize(
                    const std::vector<std::uint8_t> &payload,
                    std::size_t &offset)
                {
                    if (offset > payload.size())
                    {
                        return core::Result<std::tuple<First, Rest...>>::FromError(
                            MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }

                    auto firstResult = Serializer<First>::DeserializeAt(
                        payload.data() + offset,
                        payload.size() - offset);

                    if (!firstResult.HasValue())
                    {
                        return core::Result<std::tuple<First, Rest...>>::FromError(
                            firstResult.Error());
                    }

                    offset += firstResult.Value().second;

                    auto restResult =
                        TupleDeserializer<Rest...>::Deserialize(payload, offset);

                    if (!restResult.HasValue())
                    {
                        return core::Result<std::tuple<First, Rest...>>::FromError(
                            restResult.Error());
                    }

                    return core::Result<std::tuple<First, Rest...>>::FromValue(
                        std::tuple_cat(
                            std::make_tuple(std::move(firstResult).Value().first),
                            std::move(restResult).Value()));
                }
            };

            /// @brief C++14 equivalent of std::apply: expands tuple elements as function args.
            template <typename F, typename Tuple, std::size_t... Is>
            auto ApplyTupleImpl(F &&f, Tuple &&t, std::index_sequence<Is...>)
                -> decltype(std::forward<F>(f)(
                    std::get<Is>(std::forward<Tuple>(t))...))
            {
                return std::forward<F>(f)(
                    std::get<Is>(std::forward<Tuple>(t))...);
            }

            template <typename F, typename Tuple>
            auto ApplyTuple(F &&f, Tuple &&t)
                -> decltype(ApplyTupleImpl(
                    std::forward<F>(f),
                    std::forward<Tuple>(t),
                    std::make_index_sequence<
                        std::tuple_size<typename std::decay<Tuple>::type>::value>{}))
            {
                return ApplyTupleImpl(
                    std::forward<F>(f),
                    std::forward<Tuple>(t),
                    std::make_index_sequence<
                        std::tuple_size<typename std::decay<Tuple>::type>::value>{});
            }
        } // namespace detail

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

        // ─────────────────────────────────────────────────────────────────────
        // SkeletonMethod — skeleton-side typed method handler per AUTOSAR AP
        // SWS_CM_00404.  Generated skeleton classes declare SkeletonMethod<R(Args...)>
        // members and register application handlers via SetHandler().
        // ─────────────────────────────────────────────────────────────────────

        /// @brief Primary template declaration for SkeletonMethod.
        ///        Usage: SkeletonMethod<ReturnType(Arg1, Arg2, ...)>
        template <typename Signature>
        class SkeletonMethod;

        /// @brief Skeleton-side method wrapper per AUTOSAR AP SWS_CM_00404.
        ///        Receives serialized requests from the transport binding,
        ///        deserializes arguments, dispatches to the registered typed handler,
        ///        and serializes the result back.
        /// @tparam R Return type
        /// @tparam Args Argument types
        template <typename R, typename... Args>
        class SkeletonMethod<R(Args...)>
        {
        private:
            std::unique_ptr<internal::SkeletonMethodBinding> mBinding;
            std::recursive_mutex *mDispatchMutex{nullptr};

        public:
            /// @brief Typed async handler: receives typed args, returns Future<R>.
            using Handler = std::function<core::Future<R>(const Args &...)>;

            /// @brief Constructs a skeleton method wrapper bound to transport implementation.
            /// @param binding Concrete skeleton method binding implementation.
            /// @param dispatchMutex Optional serialization mutex for kEventSingleThread.
            explicit SkeletonMethod(
                std::unique_ptr<internal::SkeletonMethodBinding> binding,
                std::recursive_mutex *dispatchMutex = nullptr) noexcept
                : mBinding{std::move(binding)},
                  mDispatchMutex{dispatchMutex}
            {
            }

            SkeletonMethod(const SkeletonMethod &) = delete;
            SkeletonMethod &operator=(const SkeletonMethod &) = delete;
            SkeletonMethod(SkeletonMethod &&) noexcept = default;
            SkeletonMethod &operator=(SkeletonMethod &&) noexcept = default;

            /// @brief Register the typed method handler.
            /// @param handler Application callback receiving typed arguments and
            ///        returning a Future<R> that resolves with the response value.
            /// @returns `Result<void>` indicating registration success or failure.
            core::Result<void> SetHandler(Handler handler)
            {
                if (!mBinding)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                return mBinding->Register(
                    [h = std::move(handler), mtx = mDispatchMutex](
                        const std::vector<std::uint8_t> &request)
                        -> core::Result<std::vector<std::uint8_t>>
                    {
                        // kEventSingleThread: serialize concurrent calls
                        std::unique_lock<std::recursive_mutex> guard;
                        if (mtx)
                        {
                            guard = std::unique_lock<std::recursive_mutex>(*mtx);
                        }

                        std::size_t offset = 0;
                        auto argsResult =
                            detail::TupleDeserializer<Args...>::Deserialize(
                                request, offset);

                        if (!argsResult.HasValue())
                        {
                            return core::Result<std::vector<std::uint8_t>>::FromError(
                                argsResult.Error());
                        }

                        auto future =
                            detail::ApplyTuple(h, std::move(argsResult).Value());
                        auto result = future.GetResult();

                        if (!result.HasValue())
                        {
                            return core::Result<std::vector<std::uint8_t>>::FromError(
                                result.Error());
                        }

                        return core::Result<std::vector<std::uint8_t>>::FromValue(
                            Serializer<R>::Serialize(result.Value()));
                    });
            }

            /// @brief Unregister the method handler.
            void UnsetHandler()
            {
                if (mBinding)
                {
                    mBinding->Unregister();
                }
            }
        };

        /// @brief Skeleton-side method wrapper for void-return methods.
        /// @tparam Args Argument types
        template <typename... Args>
        class SkeletonMethod<void(Args...)>
        {
        private:
            std::unique_ptr<internal::SkeletonMethodBinding> mBinding;
            std::recursive_mutex *mDispatchMutex{nullptr};

        public:
            /// @brief Typed async handler for void-return methods.
            using Handler = std::function<core::Future<void>(const Args &...)>;

            /// @brief Constructs a void-return skeleton method wrapper.
            /// @param binding Concrete skeleton method binding implementation.
            /// @param dispatchMutex Optional serialization mutex for kEventSingleThread.
            explicit SkeletonMethod(
                std::unique_ptr<internal::SkeletonMethodBinding> binding,
                std::recursive_mutex *dispatchMutex = nullptr) noexcept
                : mBinding{std::move(binding)},
                  mDispatchMutex{dispatchMutex}
            {
            }

            SkeletonMethod(const SkeletonMethod &) = delete;
            SkeletonMethod &operator=(const SkeletonMethod &) = delete;
            SkeletonMethod(SkeletonMethod &&) noexcept = default;
            SkeletonMethod &operator=(SkeletonMethod &&) noexcept = default;

            /// @brief Register the typed void-return method handler.
            /// @param handler Application callback receiving typed arguments.
            /// @returns `Result<void>` indicating registration success or failure.
            core::Result<void> SetHandler(Handler handler)
            {
                if (!mBinding)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                return mBinding->Register(
                    [h = std::move(handler), mtx = mDispatchMutex](
                        const std::vector<std::uint8_t> &request)
                        -> core::Result<std::vector<std::uint8_t>>
                    {
                        std::unique_lock<std::recursive_mutex> guard;
                        if (mtx)
                        {
                            guard = std::unique_lock<std::recursive_mutex>(*mtx);
                        }

                        std::size_t offset = 0;
                        auto argsResult =
                            detail::TupleDeserializer<Args...>::Deserialize(
                                request, offset);

                        if (!argsResult.HasValue())
                        {
                            return core::Result<std::vector<std::uint8_t>>::FromError(
                                argsResult.Error());
                        }

                        auto future =
                            detail::ApplyTuple(h, std::move(argsResult).Value());
                        auto result = future.GetResult();

                        if (!result.HasValue())
                        {
                            return core::Result<std::vector<std::uint8_t>>::FromError(
                                result.Error());
                        }

                        return core::Result<std::vector<std::uint8_t>>::FromValue({});
                    });
            }

            /// @brief Unregister the method handler.
            void UnsetHandler()
            {
                if (mBinding)
                {
                    mBinding->Unregister();
                }
            }
        };

        // ─────────────────────────────────────────────────────────────────────
        // FireAndForget methods — send only, no response expected
        // Per AUTOSAR AP SWS_CM_00196: fire-and-forget methods have no reply.
        // The proxy sends and immediately returns; the skeleton handles and
        // never replies.
        // ─────────────────────────────────────────────────────────────────────

        /// @brief Primary template declaration for ProxyFireAndForgetMethod.
        ///        Usage: ProxyFireAndForgetMethod<Arg1, Arg2, ...>
        template <typename... Args>
        class ProxyFireAndForgetMethod
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
            /// @brief Creates a fire-and-forget proxy method wrapper.
            /// @param binding Concrete method binding implementation.
            explicit ProxyFireAndForgetMethod(
                std::unique_ptr<internal::ProxyMethodBinding> binding) noexcept
                : mBinding{std::move(binding)}
            {
            }

            ProxyFireAndForgetMethod(const ProxyFireAndForgetMethod &) = delete;
            ProxyFireAndForgetMethod &operator=(
                const ProxyFireAndForgetMethod &) = delete;
            ProxyFireAndForgetMethod(ProxyFireAndForgetMethod &&) noexcept = default;
            ProxyFireAndForgetMethod &operator=(
                ProxyFireAndForgetMethod &&) noexcept = default;

            /// @brief Send the method arguments; does not wait for a response.
            /// @param args Method arguments.
            void operator()(Args... args)
            {
                if (!mBinding)
                {
                    return;
                }

                auto payload = SerializeArgs(args...);
                // Fire and forget: response handler discards any reply
                mBinding->Call(
                    payload,
                    [](core::Result<std::vector<std::uint8_t>>) {});
            }
        };

        /// @brief Primary template declaration for SkeletonFireAndForgetMethod.
        ///        Usage: SkeletonFireAndForgetMethod<Arg1, Arg2, ...>
        template <typename... Args>
        class SkeletonFireAndForgetMethod
        {
        private:
            std::unique_ptr<internal::SkeletonMethodBinding> mBinding;
            std::recursive_mutex *mDispatchMutex{nullptr};

        public:
            /// @brief Typed handler — receives args, returns nothing.
            using Handler = std::function<void(const Args &...)>;

            /// @brief Constructs a fire-and-forget skeleton method wrapper.
            /// @param binding Concrete skeleton method binding implementation.
            /// @param dispatchMutex Optional serialization mutex for kEventSingleThread.
            explicit SkeletonFireAndForgetMethod(
                std::unique_ptr<internal::SkeletonMethodBinding> binding,
                std::recursive_mutex *dispatchMutex = nullptr) noexcept
                : mBinding{std::move(binding)},
                  mDispatchMutex{dispatchMutex}
            {
            }

            SkeletonFireAndForgetMethod(
                const SkeletonFireAndForgetMethod &) = delete;
            SkeletonFireAndForgetMethod &operator=(
                const SkeletonFireAndForgetMethod &) = delete;
            SkeletonFireAndForgetMethod(
                SkeletonFireAndForgetMethod &&) noexcept = default;
            SkeletonFireAndForgetMethod &operator=(
                SkeletonFireAndForgetMethod &&) noexcept = default;

            /// @brief Register the typed fire-and-forget handler.
            /// @param handler Application callback receiving typed arguments.
            ///        The handler does not return a value; the binding sends an
            ///        empty success reply so the transport framing is consistent.
            /// @returns `Result<void>` indicating registration success or failure.
            core::Result<void> SetHandler(Handler handler)
            {
                if (!mBinding)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ComErrc::kServiceNotOffered));
                }

                return mBinding->Register(
                    [h = std::move(handler), mtx = mDispatchMutex](
                        const std::vector<std::uint8_t> &request)
                        -> core::Result<std::vector<std::uint8_t>>
                    {
                        std::unique_lock<std::recursive_mutex> guard;
                        if (mtx)
                        {
                            guard = std::unique_lock<std::recursive_mutex>(*mtx);
                        }

                        std::size_t offset = 0;
                        auto argsResult =
                            detail::TupleDeserializer<Args...>::Deserialize(
                                request, offset);

                        if (!argsResult.HasValue())
                        {
                            return core::Result<std::vector<std::uint8_t>>::FromError(
                                argsResult.Error());
                        }

                        detail::ApplyTuple(h, std::move(argsResult).Value());
                        // Fire-and-forget: return empty success payload
                        return core::Result<std::vector<std::uint8_t>>::FromValue({});
                    });
            }

            /// @brief Unregister the handler.
            void UnsetHandler()
            {
                if (mBinding)
                {
                    mBinding->Unregister();
                }
            }
        };
    }
}

#endif
