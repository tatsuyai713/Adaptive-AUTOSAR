#ifndef ARA_COM_INTERNAL_BINDING_FACTORY_H
#define ARA_COM_INTERNAL_BINDING_FACTORY_H

#include <cstdint>
#include <memory>
#include "./event_binding.h"
#include "./method_binding.h"
#include "./vsomeip_event_binding.h"
#include "../com_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace internal
        {
            /// @brief Transport binding selection
            enum class TransportBinding : std::uint8_t
            {
                kVsomeip = 0U,
                kCycloneDds = 1U,
                kIceoryx = 2U
            };

            /// @brief Factory for creating transport-specific binding instances.
            ///        Abstracts backend selection from the typed wrappers.
            class BindingFactory
            {
            public:
                /// @brief Create a proxy-side event binding
                static std::unique_ptr<ProxyEventBinding> CreateProxyEventBinding(
                    TransportBinding transport,
                    const EventBindingConfig &config)
                {
                    switch (transport)
                    {
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipProxyEventBinding>(config);
                    default:
                        return nullptr;
                    }
                }

                /// @brief Create a skeleton-side event binding
                static std::unique_ptr<SkeletonEventBinding> CreateSkeletonEventBinding(
                    TransportBinding transport,
                    const EventBindingConfig &config)
                {
                    switch (transport)
                    {
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipSkeletonEventBinding>(config);
                    default:
                        return nullptr;
                    }
                }
            };
        }
    }
}

#endif
