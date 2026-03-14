/// @file src/ara/com/internal/binding_factory.h
/// @brief Declarations for binding factory.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_BINDING_FACTORY_H
#define ARA_COM_INTERNAL_BINDING_FACTORY_H

#include <cstdint>
#include <memory>
#include "./event_binding.h"
#include "./method_binding.h"
#if ARA_COM_USE_VSOMEIP
#include "./vsomeip_event_binding.h"
#include "./vsomeip_method_binding.h"
#endif
#if ARA_COM_USE_ICEORYX
#include "./iceoryx_event_binding.h"
#include "./iceoryx_method_binding.h"
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include "./dds_event_binding.h"
#include "./dds_method_binding.h"
#endif
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
#if ARA_COM_USE_VSOMEIP
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipProxyEventBinding>(config);
#endif
#if ARA_COM_USE_ICEORYX
                    case TransportBinding::kIceoryx:
                        return std::make_unique<IceoryxProxyEventBinding>(config);
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    case TransportBinding::kCycloneDds:
                        return std::make_unique<DdsProxyEventBinding>(config);
#endif
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
#if ARA_COM_USE_VSOMEIP
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipSkeletonEventBinding>(config);
#endif
#if ARA_COM_USE_ICEORYX
                    case TransportBinding::kIceoryx:
                        return std::make_unique<IceoryxSkeletonEventBinding>(config);
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    case TransportBinding::kCycloneDds:
                        return std::make_unique<DdsSkeletonEventBinding>(config);
#endif
                    default:
                        return nullptr;
                    }
                }

                /// @brief Create a proxy-side method binding
                static std::unique_ptr<ProxyMethodBinding> CreateProxyMethodBinding(
                    TransportBinding transport,
                    const MethodBindingConfig &config)
                {
                    switch (transport)
                    {
#if ARA_COM_USE_VSOMEIP
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipProxyMethodBinding>(config);
#endif
#if ARA_COM_USE_ICEORYX
                    case TransportBinding::kIceoryx:
                        return std::make_unique<IceoryxProxyMethodBinding>(config);
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    case TransportBinding::kCycloneDds:
                        return std::make_unique<DdsProxyMethodBinding>(config);
#endif
                    default:
                        return nullptr;
                    }
                }

                /// @brief Create a skeleton-side method binding
                static std::unique_ptr<SkeletonMethodBinding> CreateSkeletonMethodBinding(
                    TransportBinding transport,
                    const MethodBindingConfig &config)
                {
                    switch (transport)
                    {
#if ARA_COM_USE_VSOMEIP
                    case TransportBinding::kVsomeip:
                        return std::make_unique<VsomeipSkeletonMethodBinding>(config);
#endif
#if ARA_COM_USE_ICEORYX
                    case TransportBinding::kIceoryx:
                        return std::make_unique<IceoryxSkeletonMethodBinding>(config);
#endif
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    case TransportBinding::kCycloneDds:
                        return std::make_unique<DdsSkeletonMethodBinding>(config);
#endif
                    default:
                        return nullptr;
                    }
                }
            };
        }
    }
}

#endif
