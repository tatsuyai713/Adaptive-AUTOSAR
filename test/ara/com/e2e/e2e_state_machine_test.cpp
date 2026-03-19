#include <gtest/gtest.h>
#include "../../../../src/ara/com/e2e/e2e_state_machine.h"

namespace ara
{
    namespace com
    {
        namespace e2e
        {
            TEST(E2EStateMachineTest, DefaultConstruction)
            {
                E2EStateMachine sm;
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kNoData);
                EXPECT_EQ(sm.GetOkCount(), 0U);
                EXPECT_EQ(sm.GetErrorCount(), 0U);
            }

            TEST(E2EStateMachineTest, TransitionFromNoDataToInit)
            {
                E2EStateMachine sm;
                sm.Check(CheckStatusType::kOk);
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kInit);
            }

            TEST(E2EStateMachineTest, TransitionToValid)
            {
                E2E_SMConfig config;
                config.windowSize = 5;
                config.minOkStateInit = 3;
                config.maxErrorStateInit = 2;
                E2EStateMachine sm(config);

                // Feed enough OKs to transition to Valid
                for (int i = 0; i < 4; ++i)
                {
                    sm.Check(CheckStatusType::kOk);
                }
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kValid);
            }

            TEST(E2EStateMachineTest, TransitionToInvalid)
            {
                E2E_SMConfig config;
                config.windowSize = 5;
                config.minOkStateInit = 3;
                config.maxErrorStateInit = 1;
                E2EStateMachine sm(config);

                sm.Check(CheckStatusType::kOk);
                sm.Check(CheckStatusType::kWrongCrc);
                sm.Check(CheckStatusType::kWrongCrc);

                // Should have transitioned to Invalid after exceeding maxErrorStateInit
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kInvalid);
            }

            TEST(E2EStateMachineTest, ResetToNoData)
            {
                E2EStateMachine sm;
                sm.Check(CheckStatusType::kOk);
                EXPECT_NE(sm.GetState(), E2E_SMCheckStatusType::kNoData);

                sm.Reset();
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kNoData);
                EXPECT_EQ(sm.GetOkCount(), 0U);
                EXPECT_EQ(sm.GetErrorCount(), 0U);
            }

            TEST(E2EStateMachineTest, OkAndErrorCounts)
            {
                E2EStateMachine sm;
                sm.Check(CheckStatusType::kOk);
                sm.Check(CheckStatusType::kWrongCrc);
                sm.Check(CheckStatusType::kOk);

                EXPECT_EQ(sm.GetOkCount(), 2U);
                EXPECT_EQ(sm.GetErrorCount(), 1U);
            }

            TEST(E2EStateMachineTest, ConfigCustomValues)
            {
                E2E_SMConfig config;
                config.windowSize = 10;
                config.minOkStateInit = 5;
                config.maxErrorStateInit = 3;
                E2EStateMachine sm(config);
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kNoData);
            }

            // ── SMState tests (SWS_CM_00010) ──

            TEST(E2EStateMachineTest, DefaultSMStateEnabled)
            {
                E2EStateMachine sm;
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineEnabled);
            }

            TEST(E2EStateMachineTest, DisableSMState)
            {
                E2EStateMachine sm;
                sm.Disable();
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineDisabled);
            }

            TEST(E2EStateMachineTest, EnableAfterDisable)
            {
                E2EStateMachine sm;
                sm.Disable();
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineDisabled);

                sm.Enable();
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineEnabled);
            }

            TEST(E2EStateMachineTest, DisabledSMStateSkipsCheck)
            {
                E2EStateMachine sm;
                sm.Disable();

                // Checks should be ignored when disabled
                sm.Check(CheckStatusType::kOk);
                sm.Check(CheckStatusType::kOk);
                sm.Check(CheckStatusType::kOk);

                // State should remain NoData since Check was skipped
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kNoData);
                EXPECT_EQ(sm.GetOkCount(), 0U);
                EXPECT_EQ(sm.GetErrorCount(), 0U);
            }

            TEST(E2EStateMachineTest, ReEnableResumesChecking)
            {
                E2EStateMachine sm;
                sm.Disable();
                sm.Check(CheckStatusType::kOk);
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kNoData);

                sm.Enable();
                sm.Check(CheckStatusType::kOk);
                EXPECT_EQ(sm.GetState(), E2E_SMCheckStatusType::kInit);
                EXPECT_EQ(sm.GetOkCount(), 1U);
            }

            TEST(E2EStateMachineTest, ResetDoesNotAffectSMState)
            {
                E2EStateMachine sm;
                sm.Disable();
                sm.Reset();
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineDisabled);

                sm.Enable();
                sm.Reset();
                EXPECT_EQ(sm.GetSMState(), SMState::kStateMachineEnabled);
            }

            TEST(SMStateEnumTest, EnumValues)
            {
                EXPECT_EQ(
                    static_cast<std::uint8_t>(SMState::kStateMachineDisabled), 0U);
                EXPECT_EQ(
                    static_cast<std::uint8_t>(SMState::kStateMachineEnabled), 1U);
            }

            TEST(E2E_SMCheckStatusTypeTest, EnumValues)
            {
                EXPECT_EQ(
                    static_cast<std::uint8_t>(E2E_SMCheckStatusType::kValid), 0U);
                EXPECT_EQ(
                    static_cast<std::uint8_t>(E2E_SMCheckStatusType::kNoData), 1U);
                EXPECT_EQ(
                    static_cast<std::uint8_t>(E2E_SMCheckStatusType::kInit), 2U);
                EXPECT_EQ(
                    static_cast<std::uint8_t>(E2E_SMCheckStatusType::kInvalid), 3U);
                EXPECT_EQ(
                    static_cast<std::uint8_t>(E2E_SMCheckStatusType::kRepeat), 4U);
            }

            TEST(E2E_SMConfigTest, DefaultValues)
            {
                E2E_SMConfig config;
                EXPECT_EQ(config.windowSize, 5U);
                EXPECT_EQ(config.minOkStateInit, 3U);
                EXPECT_EQ(config.maxErrorStateInit, 2U);
                EXPECT_EQ(config.minOkStateValid, 3U);
                EXPECT_EQ(config.maxErrorStateValid, 2U);
                EXPECT_EQ(config.minOkStateInvalid, 3U);
                EXPECT_EQ(config.maxErrorStateInvalid, 5U);
            }
        }
    }
}
