#ifndef ARA_COM_PUBSUB_AUTOSAR_PORTABLE_API_H
#define ARA_COM_PUBSUB_AUTOSAR_PORTABLE_API_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "ara/com/zerocopy/zero_copy_binding.h"
#include "ara/core/instance_specifier.h"
#include "ara/core/result.h"

namespace sample
{
    namespace ara_com_pubsub
    {
        namespace portable
        {
            // Transport-neutral event backend selector used by sample CLI.
            enum class EventBackend : std::uint8_t
            {
                kSomeIp = 0U,
                kDds = 1U,
                kCycloneDds = kDds
            };

            // Transport-neutral zero-copy selector for local shared-memory path.
            enum class ZeroCopyBackend : std::uint8_t
            {
                kNone = 0U,
                kZeroCopy = 1U,
                kIceoryx = kZeroCopy
            };

            // Runtime binding profile for one provider/consumer instance.
            struct BackendProfile
            {
                EventBackend EventBinding{EventBackend::kSomeIp};
                ZeroCopyBackend ZeroCopyBinding{ZeroCopyBackend::kNone};
                std::uint32_t DdsDomainId{0U};
                std::string DdsTopicName{
                    "adaptive_autosar/sample/ara_com_pubsub/VehicleStatusFrame"};
            };

            const char *ToString(EventBackend backend) noexcept;
            const char *ToString(ZeroCopyBackend backend) noexcept;

            ara::core::Result<EventBackend> ParseEventBackend(
                std::string backendName);
            ara::core::Result<ZeroCopyBackend> ParseZeroCopyBackend(
                std::string backendName);

            // Discovery result independent from concrete middleware handle types.
            struct VehicleStatusServiceHandle
            {
                std::uint16_t ServiceId;
                std::uint16_t InstanceId;
                EventBackend EventBinding{EventBackend::kSomeIp};
            };

            using FindServiceHandler =
                std::function<void(std::vector<VehicleStatusServiceHandle>)>;
            using EventReceiveHandler =
                std::function<void(const std::vector<std::uint8_t> &)>;
            using SubscriptionStateHandler =
                std::function<bool(std::uint16_t clientId, bool subscribed)>;
            using SubscriptionStatusHandler =
                std::function<void(std::uint16_t statusCode)>;

            // Provider API mirroring ara::com skeleton-oriented lifecycle.
            class VehicleStatusProvider
            {
            private:
                class Impl;
                std::unique_ptr<Impl> mImpl;

            public:
                explicit VehicleStatusProvider(
                    ara::core::InstanceSpecifier specifier,
                    BackendProfile profile = BackendProfile{});
                ~VehicleStatusProvider() noexcept;

                VehicleStatusProvider(const VehicleStatusProvider &) = delete;
                VehicleStatusProvider &operator=(const VehicleStatusProvider &) = delete;

                VehicleStatusProvider(VehicleStatusProvider &&other) noexcept;
                VehicleStatusProvider &operator=(VehicleStatusProvider &&other) noexcept;

                EventBackend GetEventBackend() const noexcept;
                ZeroCopyBackend GetZeroCopyBackend() const noexcept;

                ara::core::Result<void> OfferService();
                void StopOfferService();

                ara::core::Result<void> OfferEvent();
                void StopOfferEvent();

                ara::core::Result<void> NotifyEvent(
                    const std::vector<std::uint8_t> &payload,
                    bool force = true) const;

                ara::core::Result<void> SetSubscriptionStateHandler(
                    SubscriptionStateHandler handler);
                void UnsetSubscriptionStateHandler();

                ara::core::Result<ara::com::zerocopy::ZeroCopyPublisher>
                CreateZeroCopyPublisher(
                    std::string runtimeName = "adaptive_autosar_ara_com",
                    std::uint64_t historyCapacity = 0U) const;
            };

            // Consumer API mirroring ara::com proxy-oriented lifecycle.
            class VehicleStatusConsumer
            {
            private:
                class Impl;
                std::unique_ptr<Impl> mImpl;

            public:
                enum class SubscriptionState : std::uint8_t
                {
                    kNotSubscribed = 0U,
                    kSubscriptionPending = 1U,
                    kSubscribed = 2U
                };

                static ara::core::Result<void> StartFindService(
                    FindServiceHandler handler,
                    BackendProfile profile = BackendProfile{});
                static void StopFindService();

                explicit VehicleStatusConsumer(
                    VehicleStatusServiceHandle handle,
                    BackendProfile profile = BackendProfile{});
                ~VehicleStatusConsumer() noexcept;

                VehicleStatusConsumer(const VehicleStatusConsumer &) = delete;
                VehicleStatusConsumer &operator=(const VehicleStatusConsumer &) = delete;

                VehicleStatusConsumer(VehicleStatusConsumer &&other) noexcept;
                VehicleStatusConsumer &operator=(VehicleStatusConsumer &&other) noexcept;

                EventBackend GetEventBackend() const noexcept;
                ZeroCopyBackend GetZeroCopyBackend() const noexcept;

                ara::core::Result<void> Subscribe(
                    EventReceiveHandler handler,
                    std::uint8_t majorVersion = 1U);
                ara::core::Result<void> Subscribe(
                    std::uint8_t majorVersion = 1U);
                void Unsubscribe();

                ara::core::Result<void> SetReceiveHandler(
                    EventReceiveHandler handler);
                void UnsetReceiveHandler();

                ara::core::Result<std::size_t> GetNewSamples(
                    std::size_t maxSamples,
                    EventReceiveHandler handler);

                ara::core::Result<void> SetSampleQueueLimit(
                    std::size_t maxSamples);

                SubscriptionState GetSubscriptionState() const noexcept;
                bool HasSampleQueueOverflowViolation() const noexcept;
                void ClearSampleQueueOverflowViolation() noexcept;

                ara::core::Result<void> SetSubscriptionStatusHandler(
                    SubscriptionStatusHandler handler);
                void UnsetSubscriptionStatusHandler();

                ara::core::Result<ara::com::zerocopy::ZeroCopySubscriber>
                CreateZeroCopySubscriber(
                    std::string runtimeName = "adaptive_autosar_ara_com",
                    std::uint64_t queueCapacity = 256U,
                    std::uint64_t historyRequest = 0U) const;
            };
        }
    }
}

#endif
