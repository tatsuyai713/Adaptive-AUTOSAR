#include "./recovery_action_dispatcher.h"

namespace ara
{
    namespace phm
    {
        core::Result<void> RecoveryActionDispatcher::Register(
            const std::string &name,
            RecoveryAction *action)
        {
            if (!action || name.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kInvalidArgument));
            }

            auto _result{mActions.emplace(name, action)};
            if (!_result.second)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kAlreadyExists));
            }

            return core::Result<void>{};
        }

        core::Result<void> RecoveryActionDispatcher::Unregister(const std::string &name)
        {
            const auto cRemoved{mActions.erase(name)};
            if (cRemoved == 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kNotFound));
            }

            return core::Result<void>{};
        }

        core::Result<void> RecoveryActionDispatcher::Dispatch(
            const std::string &name,
            const exec::ExecutionErrorEvent &executionError,
            TypeOfSupervision supervision)
        {
            auto _iterator{mActions.find(name)};
            if (_iterator == mActions.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PhmErrc::kNotFound));
            }

            _iterator->second->RecoveryHandler(executionError, supervision);
            return core::Result<void>{};
        }

        std::size_t RecoveryActionDispatcher::GetActionCount() const noexcept
        {
            return mActions.size();
        }
    }
}
