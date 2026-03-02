/// @file src/ara/sm/function_group_state_machine.h
/// @brief Function Group state machine with guard conditions, timeout transitions,
///        and transition history.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef FUNCTION_GROUP_STATE_MACHINE_H
#define FUNCTION_GROUP_STATE_MACHINE_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./sm_error_domain.h"

namespace ara
{
    namespace sm
    {
        /// @brief Definition of a single state-to-state transition.
        struct FunctionGroupTransition
        {
            /// Source state name (empty = match any state).
            std::string fromState;
            /// Target state name.
            std::string toState;
            /// Optional guard: returns true when the transition is allowed.
            /// If not set, the transition is always allowed.
            std::function<bool()> guardFn;
            /// Optional timeout: the machine automatically attempts this transition
            /// after the source state has been active for `timeoutMs` milliseconds
            /// (0 = disabled).
            std::uint64_t timeoutMs{0U};
        };

        /// @brief One entry in the transition history ring buffer.
        struct TransitionRecord
        {
            std::string fromState;
            std::string toState;
            /// Steady-clock time when the transition completed.
            std::chrono::steady_clock::time_point timestamp;
        };

        /// @brief Function Group state machine.
        ///
        /// Features:
        ///  - Explicit state set with `AddState()`.
        ///  - Directional transitions with optional guard conditions.
        ///  - Timeout-based automatic transitions (evaluated via `Tick()`).
        ///  - State-change notification callback.
        ///  - Fixed-size ring buffer of recent transitions (`GetTransitionHistory()`).
        class FunctionGroupStateMachine
        {
        public:
            /// @brief Callback invoked after a state transition.
            using StateChangeHandler =
                std::function<void(const std::string &fromState,
                                   const std::string &toState)>;

            /// @brief Maximum number of records kept in the transition history.
            static constexpr std::size_t cHistorySize{32U};

            FunctionGroupStateMachine() noexcept = default;

            /// @brief Add a valid state to the machine.
            /// @param stateName  State identifier (must be unique).
            /// @returns Void on success, `kInvalidArgument` if name is empty or
            ///          duplicate.
            core::Result<void> AddState(const std::string &stateName);

            /// @brief Add a transition definition.
            /// @param transition  Transition descriptor.
            /// @returns Void on success, `kInvalidArgument` if toState is unknown.
            core::Result<void> AddTransition(FunctionGroupTransition transition);

            /// @brief Set the initial (current) state without triggering a callback.
            /// @param stateName  Must have been registered via `AddState()`.
            /// @returns Void on success, or error if unknown.
            core::Result<void> SetCurrentState(const std::string &stateName);

            /// @brief Attempt an explicit transition to `toState`.
            ///
            /// Scans transitions from the current state (or wildcard fromState).
            /// The first matching transition whose guard passes is executed.
            /// @returns Void on success, `kTransitionFailed` if no matching
            ///          transition exists or all guards fail.
            core::Result<void> TryTransition(const std::string &toState);

            /// @brief Drive timeout-based automatic transitions.
            ///
            /// Call this periodically (e.g., every 100 ms) with the current
            /// monotonic time. If the current state has been active at least
            /// `timeoutMs` milliseconds and the associated guard passes, the
            /// automatic transition fires.
            /// @param nowSteady  Current time (default: now).
            void Tick(std::chrono::steady_clock::time_point nowSteady =
                          std::chrono::steady_clock::now());

            /// @brief Get the current state name.
            std::string GetCurrentState() const;

            /// @brief Check whether a state has been registered.
            bool HasState(const std::string &stateName) const;

            /// @brief Get how long the machine has been in the current state.
            std::chrono::milliseconds GetTimeInCurrentState(
                std::chrono::steady_clock::time_point nowSteady =
                    std::chrono::steady_clock::now()) const;

            /// @brief Get up to `cHistorySize` most recent transition records.
            std::vector<TransitionRecord> GetTransitionHistory() const;

            /// @brief Set callback invoked after every state change.
            void SetStateChangeHandler(StateChangeHandler handler);

            /// @brief Clear the state change callback.
            void ClearStateChangeHandler() noexcept;

        private:
            mutable std::mutex mMutex;
            std::vector<std::string> mStates;
            std::vector<FunctionGroupTransition> mTransitions;
            std::string mCurrentState;
            std::chrono::steady_clock::time_point mStateEnteredAt;
            std::deque<TransitionRecord> mHistory;
            StateChangeHandler mHandler;

            /// Execute the actual state change (lock already held).
            /// Appends to history and schedules the callback invocation.
            std::string executeTransition(const std::string &toState,
                                          std::chrono::steady_clock::time_point now);

            bool isKnownState(const std::string &name) const;
        };
    } // namespace sm
} // namespace ara

#endif
