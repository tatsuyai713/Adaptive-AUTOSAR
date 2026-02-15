/// @file src/ara/exec/extension/non_standard.h
/// @brief Extension aliases for non-standard Execution Management APIs.
/// @details Keep this surface minimal. Standardized APIs should be consumed
///          from `ara::exec::*` directly.

#ifndef ARA_EXEC_EXTENSION_NON_STANDARD_H
#define ARA_EXEC_EXTENSION_NON_STANDARD_H

#include "../helper/process_watchdog.h"

namespace ara
{
    namespace exec
    {
        namespace extension
        {
            /// @brief Alias namespace for helper utilities.
            namespace helper = ara::exec::helper;

            /// @brief Alias to repository-specific process watchdog helper.
            using ProcessWatchdog = ara::exec::helper::ProcessWatchdog;
        }
    }
}

#endif
