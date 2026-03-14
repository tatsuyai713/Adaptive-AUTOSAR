/// @file src/ara/sm/update_request_handler.h
/// @brief Update request coordination handler (SWS_SM_91003).

#ifndef ARA_SM_UPDATE_REQUEST_HANDLER_H
#define ARA_SM_UPDATE_REQUEST_HANDLER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Update state.
        enum class UpdateState : std::uint8_t
        {
            kIdle = 0,
            kPending = 1,
            kInProgress = 2,
            kCompleted = 3,
            kFailed = 4
        };

        /// @brief Handler for coordinating software update transitions (SWS_SM_91003).
        class UpdateRequestHandler
        {
        public:
            using UpdateCallback = std::function<void(UpdateState)>;

            UpdateRequestHandler() noexcept;

            core::Result<void> PrepareUpdate();
            core::Result<void> VerifyUpdate();
            core::Result<void> RollbackUpdate();
            UpdateState GetUpdateState() const noexcept;
            void SetUpdateCallback(UpdateCallback cb);

        private:
            UpdateState mState;
            UpdateCallback mCallback;
            mutable std::mutex mMutex;

            void notifyAndSetState(UpdateState newState);
        };

    } // namespace sm
} // namespace ara

#endif // ARA_SM_UPDATE_REQUEST_HANDLER_H
