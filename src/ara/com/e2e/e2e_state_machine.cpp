/// @file src/ara/com/e2e/e2e_state_machine.cpp
/// @brief Implementation of E2EStateMachine (SWS_E2E_00300).

#include "./e2e_state_machine.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            E2EStateMachine::E2EStateMachine(const E2E_SMConfig &config) noexcept
                : mConfig{config},
                  mState{E2E_SMCheckStatusType::kNoData},
                  mSMState{SMState::kStateMachineEnabled},
                  mOkCount{0U},
                  mErrorCount{0U},
                  mWindowCount{0U}
            {
            }

            void E2EStateMachine::Check(CheckStatusType checkStatus)
            {
                if (mSMState == SMState::kStateMachineDisabled)
                {
                    return;
                }

                ++mWindowCount;

                bool isOk = (checkStatus == CheckStatusType::kOk);
                bool isError = (checkStatus == CheckStatusType::kWrongCrc ||
                                checkStatus == CheckStatusType::kWrongSequence);

                if (isOk)
                {
                    ++mOkCount;
                }
                else if (isError)
                {
                    ++mErrorCount;
                }

                // Window management: when window fills, halve the counters
                if (mWindowCount >= mConfig.windowSize)
                {
                    mOkCount /= 2U;
                    mErrorCount /= 2U;
                    mWindowCount = 0U;
                }

                // State transitions
                switch (mState)
                {
                case E2E_SMCheckStatusType::kNoData:
                    if (isOk || isError)
                    {
                        mState = E2E_SMCheckStatusType::kInit;
                    }
                    break;

                case E2E_SMCheckStatusType::kInit:
                    if (mOkCount >= mConfig.minOkStateInit)
                    {
                        mState = E2E_SMCheckStatusType::kValid;
                    }
                    else if (mErrorCount > mConfig.maxErrorStateInit)
                    {
                        mState = E2E_SMCheckStatusType::kInvalid;
                    }
                    break;

                case E2E_SMCheckStatusType::kValid:
                    if (mErrorCount > mConfig.maxErrorStateValid)
                    {
                        mState = E2E_SMCheckStatusType::kInvalid;
                    }
                    break;

                case E2E_SMCheckStatusType::kInvalid:
                    if (mOkCount >= mConfig.minOkStateInvalid)
                    {
                        mState = E2E_SMCheckStatusType::kValid;
                    }
                    break;

                case E2E_SMCheckStatusType::kRepeat:
                    // Transient state, re-evaluate
                    if (mOkCount >= mConfig.minOkStateValid)
                    {
                        mState = E2E_SMCheckStatusType::kValid;
                    }
                    else
                    {
                        mState = E2E_SMCheckStatusType::kInvalid;
                    }
                    break;
                }
            }

            E2E_SMCheckStatusType E2EStateMachine::GetState() const noexcept
            {
                return mState;
            }

            void E2EStateMachine::Reset() noexcept
            {
                mState = E2E_SMCheckStatusType::kNoData;
                mOkCount = 0U;
                mErrorCount = 0U;
                mWindowCount = 0U;
            }

            SMState E2EStateMachine::GetSMState() const noexcept
            {
                return mSMState;
            }

            void E2EStateMachine::Enable() noexcept
            {
                mSMState = SMState::kStateMachineEnabled;
            }

            void E2EStateMachine::Disable() noexcept
            {
                mSMState = SMState::kStateMachineDisabled;
            }

            std::uint32_t E2EStateMachine::GetOkCount() const noexcept
            {
                return mOkCount;
            }

            std::uint32_t E2EStateMachine::GetErrorCount() const noexcept
            {
                return mErrorCount;
            }

        } // namespace e2e
    } // namespace com
} // namespace ara
