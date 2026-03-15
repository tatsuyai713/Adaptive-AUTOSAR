/// @file src/ara/com/someip/session_handler.cpp
/// @brief Implementation for session handler.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./session_handler.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            SessionHandler::SessionHandler(
                SessionHandlingPolicy policy) noexcept
                : mPolicy{policy}
            {
            }

            std::uint16_t SessionHandler::NextSessionId() noexcept
            {
                if (mPolicy == SessionHandlingPolicy::kInactive)
                {
                    return 0x0000U;
                }

                std::uint16_t id =
                    mNextSessionId.fetch_add(1U, std::memory_order_relaxed);

                // Wrap from 0x0000 to 0x0001 (0x0000 is reserved for inactive)
                if (id == 0x0000U)
                {
                    id = mNextSessionId.fetch_add(1U, std::memory_order_relaxed);
                }

                return id;
            }

            SessionHandlingPolicy SessionHandler::Policy() const noexcept
            {
                return mPolicy;
            }

            bool SessionHandler::RebootFlag() const noexcept
            {
                return mRebootFlag.load(std::memory_order_relaxed);
            }

            void SessionHandler::ClearRebootFlag() noexcept
            {
                mRebootFlag.store(false, std::memory_order_relaxed);
            }

            void SessionHandler::Reset() noexcept
            {
                mNextSessionId.store(1U, std::memory_order_relaxed);
                mRebootFlag.store(true, std::memory_order_relaxed);
            }
        }
    }
}
