/// @file src/ara/diag/operation_cycle.cpp
/// @brief Implementation for operation cycle.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./operation_cycle.h"

namespace ara
{
    namespace diag
    {
        OperationCycle::OperationCycle(const ara::core::InstanceSpecifier &specifier) : mSpecifier{specifier}
        {
        }

        ara::core::Result<void> OperationCycle::SetNotifier(std::function<void()> notifier)
        {
            mNotifier = notifier;
            ara::core::Result<void> _result;

            return _result;
        }

        ara::core::Result<void> OperationCycle::RestartOperationCycle()
        {
            if (mNotifier)
            {
                mNotifier();
            }
            ara::core::Result<void> _result;

            return _result;
        }
    }
}