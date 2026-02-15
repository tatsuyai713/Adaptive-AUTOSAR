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
            kNotSubscribed = 0U,
            kSubscriptionPending = 1U,
            kSubscribed = 2U
        };

        /// @brief Processing mode for incoming method calls per AUTOSAR AP SWS_CM_00198
        enum class MethodCallProcessingMode : std::uint8_t
        {
            kPoll = 0U,
            kEvent = 1U,
            kEventSingleThread = 2U
        };

        /// @brief Handle returned by StartFindService for stopping the search
        class FindServiceHandle
        {
        private:
            std::uint64_t mId;

        public:
            explicit FindServiceHandle(std::uint64_t id) noexcept : mId{id} {}

            std::uint64_t GetId() const noexcept { return mId; }

            bool operator==(const FindServiceHandle &other) const noexcept
            {
                return mId == other.mId;
            }

            bool operator!=(const FindServiceHandle &other) const noexcept
            {
                return mId != other.mId;
            }
        };

        /// @brief Container type for service handles per AUTOSAR AP SWS_CM_00302
        template <typename HandleType>
        using ServiceHandleContainer = std::vector<HandleType>;

        /// @brief Callback invoked when new event data is available (no-argument form per AP spec)
        using EventReceiveHandler = std::function<void()>;

        /// @brief Callback invoked when subscription state changes
        using SubscriptionStateChangeHandler =
            std::function<void(SubscriptionState)>;

        /// @brief Callback invoked when service availability changes
        template <typename HandleType>
        using FindServiceHandler =
            std::function<void(ServiceHandleContainer<HandleType>)>;
    }
}

#endif
