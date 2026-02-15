/// @file src/ara/com/types.h
/// @brief Declarations for types.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_TYPES_H
#define ARA_COM_TYPES_H

#include <cstdint>
#include <functional>
#include <vector>

namespace ara
{
    namespace com
    {
        /// @brief Subscription state per AUTOSAR AP SWS_CM_00310
        enum class SubscriptionState : std::uint8_t
        {
            kNotSubscribed = 0U,       ///< Event/field notifier is not subscribed.
            kSubscriptionPending = 1U, ///< Subscribe request sent, awaiting confirmation.
            kSubscribed = 2U           ///< Subscription is active.
        };

        /// @brief Processing mode for incoming method calls per AUTOSAR AP SWS_CM_00198
        enum class MethodCallProcessingMode : std::uint8_t
        {
            kPoll = 0U,              ///< Application polls for pending method calls.
            kEvent = 1U,             ///< Calls are dispatched via event-driven handling.
            kEventSingleThread = 2U  ///< Event-driven handling serialized on one thread.
        };

        /// @brief Handle returned by StartFindService for stopping the search
        class FindServiceHandle
        {
        private:
            std::uint64_t mId;

        public:
            /// @brief Creates a search-handle token.
            /// @param id Opaque identifier assigned by the discovery subsystem.
            explicit FindServiceHandle(std::uint64_t id) noexcept : mId{id} {}

            /// @brief Returns the opaque numeric handle value.
            std::uint64_t GetId() const noexcept { return mId; }

            /// @brief Equality comparison.
            bool operator==(const FindServiceHandle &other) const noexcept
            {
                return mId == other.mId;
            }

            /// @brief Inequality comparison.
            bool operator!=(const FindServiceHandle &other) const noexcept
            {
                return mId != other.mId;
            }
        };

        /// @brief Container type for service handles per AUTOSAR AP SWS_CM_00302
        template <typename HandleType>
        using ServiceHandleContainer = std::vector<HandleType>;

        /// @brief Callback invoked when new event data is available.
        /// @details This is the no-argument receive notification form defined in AP.
        using EventReceiveHandler = std::function<void()>;

        /// @brief Callback invoked when subscription state changes.
        using SubscriptionStateChangeHandler =
            std::function<void(SubscriptionState)>;

        /// @brief Callback invoked when service availability changes.
        template <typename HandleType>
        using FindServiceHandler =
            std::function<void(ServiceHandleContainer<HandleType>)>;
    }
}

#endif
