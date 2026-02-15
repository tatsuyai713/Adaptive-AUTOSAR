/// @file src/ara/com/zerocopy/iceoryx_zerocopy.h
/// @brief Declarations for iceoryx zerocopy.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ICEORYX_ZERO_COPY_H
#define ICEORYX_ZERO_COPY_H

#include "./zero_copy.h"

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            // Compatibility aliases for legacy internal/tests.
            /// @brief Backward-compatible alias for zero-copy publisher type.
            using IceoryxPublisher = ZeroCopyPublisher;
            /// @brief Backward-compatible alias for zero-copy subscriber type.
            using IceoryxSubscriber = ZeroCopySubscriber;
        }
    }
}

#endif
