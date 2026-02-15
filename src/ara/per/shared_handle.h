#ifndef SHARED_HANDLE_H
#define SHARED_HANDLE_H

#include <memory>

namespace ara
{
    namespace per
    {
        /// @brief Shared handle type per AUTOSAR AP SWS_PER_00002
        /// @tparam T Managed object type
        template <typename T>
        using SharedHandle = std::shared_ptr<T>;

        /// @brief Unique handle type per AUTOSAR AP SWS_PER_00003
        /// @tparam T Managed object type
        template <typename T>
        using UniqueHandle = std::unique_ptr<T>;
    }
}

#endif
