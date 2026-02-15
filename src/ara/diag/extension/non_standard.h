/// @file src/ara/diag/extension/non_standard.h
/// @brief Extension aliases for non-standard Diagnostic APIs.
/// @details This header intentionally keeps only non-standard extension entry
///          points. Standardized `ara::diag::*` classes shall be used directly.

#ifndef ARA_DIAG_EXTENSION_NON_STANDARD_H
#define ARA_DIAG_EXTENSION_NON_STANDARD_H

#include "./api_extensions.h"
#include "../debouncing/debouncer.h"
#include "../routing/routable_uds_service.h"

namespace ara
{
    namespace diag
    {
        namespace extension
        {
            /// @brief Alias namespace for repository-specific diagnostic routing utilities.
            namespace routing = ara::diag::routing;

            /// @brief Alias namespace for repository-specific debouncing utilities.
            namespace debouncing = ara::diag::debouncing;
        }
    }
}

#endif
