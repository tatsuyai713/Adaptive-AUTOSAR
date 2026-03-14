/// @file src/ara/sm/recovery_handler.cpp
/// @brief Implementation of RecoveryHandler (SWS_SM_91004).

#include "./recovery_handler.h"

namespace ara
{
    namespace sm
    {
        core::Result<void> RecoveryHandler::RequestRecovery(
            const std::string &functionGroup,
            RecoveryAction action)
        {
            RecoveryCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                cb = mCallback;
            }
            if (cb)
            {
                cb(functionGroup, action);
            }
            return core::Result<void>::FromValue();
        }

        void RecoveryHandler::SetRecoveryCallback(RecoveryCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(cb);
        }
    }
}
