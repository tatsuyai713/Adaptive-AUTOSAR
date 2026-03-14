/// @file src/ara/sm/update_request_handler.cpp
/// @brief Implementation of UpdateRequestHandler (SWS_SM_91003).

#include "./update_request_handler.h"

namespace ara
{
    namespace sm
    {
        UpdateRequestHandler::UpdateRequestHandler() noexcept
            : mState{UpdateState::kIdle}
        {
        }

        void UpdateRequestHandler::notifyAndSetState(UpdateState newState)
        {
            UpdateCallback cb;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mState = newState;
                cb = mCallback;
            }
            if (cb)
            {
                cb(newState);
            }
        }

        core::Result<void> UpdateRequestHandler::PrepareUpdate()
        {
            notifyAndSetState(UpdateState::kPending);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateRequestHandler::VerifyUpdate()
        {
            notifyAndSetState(UpdateState::kCompleted);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateRequestHandler::RollbackUpdate()
        {
            notifyAndSetState(UpdateState::kFailed);
            return core::Result<void>::FromValue();
        }

        UpdateState UpdateRequestHandler::GetUpdateState() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mState;
        }

        void UpdateRequestHandler::SetUpdateCallback(UpdateCallback cb)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mCallback = std::move(cb);
        }
    }
}
