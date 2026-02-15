/// @file src/ara/exec/worker_runnable.h
/// @brief Declarations for worker runnable.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef WORKER_RUNNABLE_H
#define WORKER_RUNNABLE_H

#include "./worker_thread.h"

namespace ara
{
    namespace exec
    {
        /// @brief DeterministicClient worker runnable abstract class
        /// @tparam ValueType Container element type
        template <typename ValueType>
        class WorkerRunnable
        {
        protected:
            WorkerRunnable() noexcept = default;
            virtual ~WorkerRunnable() noexcept = default;

        public:
            /// @brief Run the worker thread to process an element in the container
            /// @param element Reference to a container element
            /// @param t Reference to a worker thread for generating random numbers
            virtual void Run(ValueType &element, WorkerThread &t) = 0;
        };
    }
}

#endif