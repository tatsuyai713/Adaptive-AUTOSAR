/// @file src/ara/exec/worker_thread.cpp
/// @brief Implementation for worker thread.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./worker_thread.h"

namespace ara
{
    namespace exec
    {
        const uint64_t WorkerThread::cOffsetStep;
        std::atomic_uint64_t WorkerThread::mOffset;

        uint64_t WorkerThread::GetRandom() noexcept
        {
            const uint64_t _offset{
                mOffset.fetch_add(cOffsetStep, std::memory_order_relaxed)};
            return _offset;
        };
    }
}
