/// @file src/ara/sm/function_group_state_machine.cpp
/// @brief Implementation for FunctionGroupStateMachine.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./function_group_state_machine.h"

#include <algorithm>

namespace ara
{
    namespace sm
    {
        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        bool FunctionGroupStateMachine::isKnownState(
            const std::string &name) const
        {
            return std::find(mStates.begin(), mStates.end(), name) != mStates.end();
        }

        std::string FunctionGroupStateMachine::executeTransition(
            const std::string &toState,
            std::chrono::steady_clock::time_point now)
        {
            const std::string fromState{mCurrentState};
            mCurrentState = toState;
            mStateEnteredAt = now;

            TransitionRecord rec{fromState, toState, now};
            mHistory.push_back(rec);
            if (mHistory.size() > cHistorySize)
            {
                mHistory.pop_front();
            }

            return fromState;
        }

        // -----------------------------------------------------------------------
        // Public API
        // -----------------------------------------------------------------------

        core::Result<void> FunctionGroupStateMachine::AddState(
            const std::string &stateName)
        {
            if (stateName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};

            if (isKnownState(stateName))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kAlreadyInState));
            }

            mStates.push_back(stateName);
            return core::Result<void>::FromValue();
        }

        core::Result<void> FunctionGroupStateMachine::AddTransition(
            FunctionGroupTransition transition)
        {
            if (transition.toState.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};

            if (!isKnownState(transition.toState))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            mTransitions.push_back(std::move(transition));
            return core::Result<void>::FromValue();
        }

        core::Result<void> FunctionGroupStateMachine::SetCurrentState(
            const std::string &stateName)
        {
            if (stateName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};

            if (!isKnownState(stateName))
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidState));
            }

            mCurrentState = stateName;
            mStateEnteredAt = std::chrono::steady_clock::now();
            return core::Result<void>::FromValue();
        }

        core::Result<void> FunctionGroupStateMachine::TryTransition(
            const std::string &toState)
        {
            if (toState.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kInvalidArgument));
            }

            StateChangeHandler handlerCopy;
            std::string fromState;
            bool success{false};
            const auto now{std::chrono::steady_clock::now()};

            {
                std::lock_guard<std::mutex> lock{mMutex};

                if (!isKnownState(toState))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(SmErrc::kInvalidState));
                }

                for (const auto &t : mTransitions)
                {
                    const bool fromMatches =
                        t.fromState.empty() || t.fromState == mCurrentState;
                    if (!fromMatches || t.toState != toState)
                    {
                        continue;
                    }
                    if (t.guardFn && !t.guardFn())
                    {
                        continue;
                    }

                    fromState = executeTransition(toState, now);
                    handlerCopy = mHandler;
                    success = true;
                    break;
                }
            }

            if (!success)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(SmErrc::kTransitionFailed));
            }

            if (handlerCopy)
            {
                handlerCopy(fromState, toState);
            }

            return core::Result<void>::FromValue();
        }

        void FunctionGroupStateMachine::Tick(
            std::chrono::steady_clock::time_point nowSteady)
        {
            StateChangeHandler handlerCopy;
            std::string fromState;
            std::string toState;
            bool fired{false};

            {
                std::lock_guard<std::mutex> lock{mMutex};

                if (mCurrentState.empty())
                {
                    return;
                }

                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        nowSteady - mStateEnteredAt)
                        .count();

                for (const auto &t : mTransitions)
                {
                    if (t.timeoutMs == 0U)
                    {
                        continue;
                    }
                    const bool fromMatches =
                        t.fromState.empty() || t.fromState == mCurrentState;
                    if (!fromMatches)
                    {
                        continue;
                    }
                    if (elapsedMs < static_cast<std::int64_t>(t.timeoutMs))
                    {
                        continue;
                    }
                    if (t.guardFn && !t.guardFn())
                    {
                        continue;
                    }

                    toState = t.toState;
                    fromState = executeTransition(toState, nowSteady);
                    handlerCopy = mHandler;
                    fired = true;
                    break;
                }
            }

            if (fired && handlerCopy)
            {
                handlerCopy(fromState, toState);
            }
        }

        std::string FunctionGroupStateMachine::GetCurrentState() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mCurrentState;
        }

        bool FunctionGroupStateMachine::HasState(
            const std::string &stateName) const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return isKnownState(stateName);
        }

        std::chrono::milliseconds FunctionGroupStateMachine::GetTimeInCurrentState(
            std::chrono::steady_clock::time_point nowSteady) const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                nowSteady - mStateEnteredAt);
        }

        std::vector<TransitionRecord>
        FunctionGroupStateMachine::GetTransitionHistory() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return std::vector<TransitionRecord>(mHistory.begin(), mHistory.end());
        }

        void FunctionGroupStateMachine::SetStateChangeHandler(
            StateChangeHandler handler)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mHandler = std::move(handler);
        }

        void FunctionGroupStateMachine::ClearStateChangeHandler() noexcept
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mHandler = nullptr;
        }

    } // namespace sm
} // namespace ara
