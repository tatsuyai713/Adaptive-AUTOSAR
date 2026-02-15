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
            using IceoryxPublisher = ZeroCopyPublisher;
            using IceoryxSubscriber = ZeroCopySubscriber;
        }
    }
}

#endif
