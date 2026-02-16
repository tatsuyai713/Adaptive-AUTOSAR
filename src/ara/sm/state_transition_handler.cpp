/// @file src/ara/sm/state_transition_handler.cpp
/// @brief Implementation for state transition handler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./state_transition_handler.h"

namespace ara
{
    namespace sm
    {
        StateTransitionHandler::StateTransitionHandler()
        {
        }

        core::Result<void> StateTransitionHandler::Register(
            const std::string &functionGroup,
            TransitionCallback callback)
        {
            if (functionGroup.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            if (!callback)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};
            mHandlers[functionGroup] = std::move(callback);
            return core::Result<void>::FromValue();
        }

        void StateTransitionHandler::Unregister(const std::string &functionGroup)
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            mHandlers.erase(functionGroup);
        }

        void StateTransitionHandler::NotifyTransition(
            const std::string &functionGroup,
            const std::string &fromState,
            const std::string &toState,
            TransitionPhase phase)
        {
            TransitionCallback _callbackCopy;

            {
                std::lock_guard<std::mutex> _lock{mMutex};
                auto _it = mHandlers.find(functionGroup);
                if (_it != mHandlers.end())
                {
                    _callbackCopy = _it->second;
                }
            }

            if (_callbackCopy)
            {
                _callbackCopy(functionGroup, fromState, toState, phase);
            }
        }

        bool StateTransitionHandler::HasHandler(
            const std::string &functionGroup) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};
            return mHandlers.find(functionGroup) != mHandlers.end();
        }
    }
}
