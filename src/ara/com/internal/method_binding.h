/// @file src/ara/com/internal/method_binding.h
/// @brief Declarations for method binding.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_METHOD_BINDING_H
#define ARA_COM_INTERNAL_METHOD_BINDING_H

#include <cstdint>
#include <functional>
#include <vector>
#include "../../core/result.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief Abstract proxy-side method binding.
            ///        Sends serialized request and receives serialized response asynchronously.
            class ProxyMethodBinding
            {
            public:
                /// @brief Callback type for receiving the raw response
                using RawResponseHandler =
                    std::function<void(core::Result<std::vector<std::uint8_t>>)>;

                virtual ~ProxyMethodBinding() noexcept = default;

                /// @brief Send a method request
                /// @param requestPayload Serialized request arguments
                /// @param responseHandler Callback invoked with the response or error
                virtual void Call(
                    const std::vector<std::uint8_t> &requestPayload,
                    RawResponseHandler responseHandler) = 0;
            };

            /// @brief Abstract skeleton-side method binding.
            ///        Receives method requests and dispatches to the application handler.
            class SkeletonMethodBinding
            {
            public:
                /// @brief Handler type: receives request bytes, returns response bytes
                using RawRequestHandler =
                    std::function<core::Result<std::vector<std::uint8_t>>(
                        const std::vector<std::uint8_t> &)>;

                virtual ~SkeletonMethodBinding() noexcept = default;

                /// @brief Register a request handler for this method
                virtual core::Result<void> Register(
                    RawRequestHandler handler) = 0;

                /// @brief Unregister the request handler
                virtual void Unregister() = 0;
            };
        }
    }
}

#endif
