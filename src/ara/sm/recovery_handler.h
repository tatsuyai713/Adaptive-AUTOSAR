/// @file src/ara/sm/recovery_handler.h
/// @brief Recovery action handler (SWS_SM_91004).

#ifndef ARA_SM_RECOVERY_HANDLER_H
#define ARA_SM_RECOVERY_HANDLER_H

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
        /// @brief Recovery action type.
        enum class RecoveryAction : std::uint8_t
        {
            kNone = 0,
            kRestartProcess = 1,
            kResetFunctionGroup = 2,
            kRestartMachine = 3
        };

        /// @brief Handler for platform recovery actions (SWS_SM_91004).
        class RecoveryHandler
        {
        public:
            using RecoveryCallback = std::function<void(
                const std::string &functionGroup, RecoveryAction action)>;

            RecoveryHandler() = default;

            core::Result<void> RequestRecovery(
                const std::string &functionGroup,
                RecoveryAction action);

            void SetRecoveryCallback(RecoveryCallback cb);

        private:
            RecoveryCallback mCallback;
            mutable std::mutex mMutex;
        };

    } // namespace sm
} // namespace ara

#endif // ARA_SM_RECOVERY_HANDLER_H
