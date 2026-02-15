/// @file src/ara/com/extension/non_standard.h
/// @brief Extension aliases for non-standard Communication Management APIs.
/// @details Keep this surface minimal. Standardized APIs should be consumed
///          from `ara::com::*` directly.

#ifndef ARA_COM_EXTENSION_NON_STANDARD_H
#define ARA_COM_EXTENSION_NON_STANDARD_H

#include "../someip/sd/someip_sd_client.h"
#include "../someip/sd/someip_sd_server.h"
#include "../someip/sd/sd_network_layer.h"

namespace ara
{
    namespace com
    {
        namespace extension
        {
            /// @brief Alias namespace for repository-specific SOME/IP-SD networking.
            namespace someip_sd = ara::com::someip::sd;
        }
    }
}

#endif
