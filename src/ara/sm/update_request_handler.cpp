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
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != UpdateState::kIdle &&
                    mState != UpdateState::kCompleted &&
                    mState != UpdateState::kFailed)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kInvalidState));
                }
            }
            notifyAndSetState(UpdateState::kPending);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateRequestHandler::VerifyUpdate()
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != UpdateState::kPending &&
                    mState != UpdateState::kInProgress)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kInvalidState));
                }
            }
            notifyAndSetState(UpdateState::kInProgress);
            notifyAndSetState(UpdateState::kCompleted);
            return core::Result<void>::FromValue();
        }

        core::Result<void> UpdateRequestHandler::RollbackUpdate()
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mState != UpdateState::kPending &&
                    mState != UpdateState::kInProgress &&
                    mState != UpdateState::kFailed)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kInvalidState));
                }
            }
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
