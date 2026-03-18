/// @file src/ara/exec/application_client.cpp
/// @brief Implementation for ApplicationClient (SWS_EM_02100).

#include "./application_client.h"
#include <utility>

namespace ara
{
    namespace exec
    {
        ApplicationClient::ApplicationClient(
            core::InstanceSpecifier instanceSpecifier)
            : mInstanceSpecifier{std::move(instanceSpecifier)},
              mInstanceId{mInstanceSpecifier.ToString()}
        {
        }

        core::Result<ApplicationRecoveryAction>
        ApplicationClient::GetRecoveryAction() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return core::Result<ApplicationRecoveryAction>::FromValue(
                mPendingAction);
        }

        core::Result<void> ApplicationClient::AcknowledgeRecoveryAction()
        {
            RecoveryActionHandler handlerCopy;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                if (mPendingAction == ApplicationRecoveryAction::kNone)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(ExecErrc::kInvalidArguments));
                }
                mPendingAction = ApplicationRecoveryAction::kNone;
            }
            return core::Result<void>::FromValue();
        }

        void ApplicationClient::ReportApplicationHealth() noexcept
        {
            ++mHealthReportCount;
        }

        void ApplicationClient::SetRecoveryActionHandler(
            RecoveryActionHandler handler)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mHandler = std::move(handler);
        }

        void ApplicationClient::RequestRecoveryAction(
            ApplicationRecoveryAction action)
        {
            RecoveryActionHandler handlerCopy;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                mPendingAction = action;
                handlerCopy = mHandler;
            }
            if (handlerCopy && action != ApplicationRecoveryAction::kNone)
            {
                handlerCopy(action);
            }
        }

        std::uint64_t ApplicationClient::GetHealthReportCount() const noexcept
        {
            return mHealthReportCount.load();
        }

        const std::string &ApplicationClient::GetInstanceId() const noexcept
        {
            return mInstanceId;
        }
    }
}
