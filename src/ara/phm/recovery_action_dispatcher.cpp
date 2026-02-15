#include "./recovery_action_dispatcher.h"

namespace ara
{
    namespace phm
    {
        bool RecoveryActionDispatcher::Register(
            const std::string &name,
            RecoveryAction *action)
        {
            if (!action || name.empty())
            {
                return false;
            }

            auto _result = mActions.emplace(name, action);
            return _result.second;
        }

        bool RecoveryActionDispatcher::Unregister(const std::string &name)
        {
            return mActions.erase(name) > 0U;
        }

        bool RecoveryActionDispatcher::Dispatch(
            const std::string &name,
            const exec::ExecutionErrorEvent &executionError,
            TypeOfSupervision supervision)
        {
            auto _iterator = mActions.find(name);
            if (_iterator == mActions.end())
            {
                return false;
            }

            _iterator->second->RecoveryHandler(executionError, supervision);
            return true;
        }

        std::size_t RecoveryActionDispatcher::GetActionCount() const noexcept
        {
            return mActions.size();
        }
    }
}
