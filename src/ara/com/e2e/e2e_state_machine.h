/// @file src/ara/com/e2e/e2e_state_machine.h
/// @brief E2E state machine for communication supervision (SWS_E2E_00300-00302).

#ifndef ARA_COM_E2E_STATE_MACHINE_H
#define ARA_COM_E2E_STATE_MACHINE_H

#include <cstdint>
#include "./profile.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            /// @brief State machine check status combining E2E and SM states (SWS_E2E_00301).
            enum class E2E_SMCheckStatusType : std::uint8_t
            {
                kValid = 0,     ///< Communication functioning correctly.
                kNoData = 1,    ///< No data received yet or all data lost.
                kInit = 2,      ///< State machine is initializing.
                kInvalid = 3,   ///< Communication considered invalid/failed.
                kRepeat = 4     ///< Repeated data detected.
            };

            /// @brief E2E supervision state per SWS_CM_00010.
            ///        Represents whether the E2E state machine supervision
            ///        is enabled or disabled for a given event/field.
            enum class SMState : std::uint8_t
            {
                kStateMachineDisabled = 0U,  ///< E2E supervision is not active.
                kStateMachineEnabled = 1U    ///< E2E supervision is active.
            };

            /// @brief Configuration for the E2E state machine (SWS_E2E_00302).
            struct E2E_SMConfig
            {
                std::uint32_t windowSize{5};          ///< Size of the monitoring window.
                std::uint32_t minOkStateInit{3};      ///< Min OK results to leave Init state.
                std::uint32_t maxErrorStateInit{2};    ///< Max errors allowed in Init state.
                std::uint32_t minOkStateValid{3};     ///< Min OK results to remain Valid.
                std::uint32_t maxErrorStateValid{2};   ///< Max errors allowed in Valid state.
                std::uint32_t minOkStateInvalid{3};   ///< Min OK results to return to Valid from Invalid.
                std::uint32_t maxErrorStateInvalid{5}; ///< Max errors in Invalid before confirmed.
            };

            /// @brief E2E state machine (SWS_E2E_00300).
            /// @details Evaluates a sequence of E2E check results and determines
            ///          the overall communication status using a windowed counter.
            class E2EStateMachine
            {
            public:
                explicit E2EStateMachine(const E2E_SMConfig &config = {}) noexcept;

                /// @brief Feed a new E2E check result into the state machine.
                /// @param checkStatus The result from E2E profile verification.
                void Check(CheckStatusType checkStatus);

                /// @brief Get the current state machine status.
                E2E_SMCheckStatusType GetState() const noexcept;

                /// @brief Get the supervision enable/disable state (SWS_CM_00010).
                SMState GetSMState() const noexcept;

                /// @brief Enable the E2E state machine supervision (SWS_CM_00010).
                void Enable() noexcept;

                /// @brief Disable the E2E state machine supervision (SWS_CM_00010).
                void Disable() noexcept;

                /// @brief Reset the state machine to Init.
                void Reset() noexcept;

                /// @brief Get the number of OK results in current window.
                std::uint32_t GetOkCount() const noexcept;

                /// @brief Get the number of error results in current window.
                std::uint32_t GetErrorCount() const noexcept;

            private:
                E2E_SMConfig mConfig;
                E2E_SMCheckStatusType mState;
                SMState mSMState;
                std::uint32_t mOkCount;
                std::uint32_t mErrorCount;
                std::uint32_t mWindowCount;
            };

        } // namespace e2e
    } // namespace com
} // namespace ara

#endif // ARA_COM_E2E_STATE_MACHINE_H
