/// @file src/ara/com/someip/session_handler.h
/// @brief SOME/IP session ID handling policy.
/// @details Per PRS_SOMEIP_00030-PRS_SOMEIP_00035, the SOME/IP protocol
///          supports two session handling modes:
///          - Active:   session ID is incremented per message (wraps 0xFFFF→1)
///          - Inactive: session ID remains 0x0000
///          The reboot flag is used in SD messages to signal endpoint restart.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SOMEIP_SESSION_HANDLER_H
#define ARA_COM_SOMEIP_SESSION_HANDLER_H

#include <atomic>
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief Session handling mode per PRS_SOMEIP_00030.
            enum class SessionHandlingPolicy : std::uint8_t
            {
                /// @brief Session ID is incremented for each message.
                kActive = 0U,
                /// @brief Session ID is always 0x0000.
                kInactive = 1U
            };

            /// @brief Thread-safe session ID generator conforming to PRS_SOMEIP_00033.
            class SessionHandler
            {
            private:
                SessionHandlingPolicy mPolicy;
                std::atomic<std::uint16_t> mNextSessionId{1U};
                std::atomic<bool> mRebootFlag{true};

            public:
                /// @brief Constructor
                /// @param policy Session handling mode
                explicit SessionHandler(
                    SessionHandlingPolicy policy =
                        SessionHandlingPolicy::kActive) noexcept;

                /// @brief Get the next session ID.
                /// @details In Active mode, returns a monotonically increasing
                ///          value (1..0xFFFF, wrapping from 0xFFFF to 1).
                ///          In Inactive mode, always returns 0x0000.
                std::uint16_t NextSessionId() noexcept;

                /// @brief Get the current session handling policy.
                SessionHandlingPolicy Policy() const noexcept;

                /// @brief Get the reboot detection flag.
                /// @details The reboot flag starts as true and is cleared by
                ///          the caller once the first SD message has been sent.
                bool RebootFlag() const noexcept;

                /// @brief Clear the reboot flag (after first SD message sent).
                void ClearRebootFlag() noexcept;

                /// @brief Reset session handler (simulates endpoint reboot).
                void Reset() noexcept;
            };
        }
    }
}

#endif
