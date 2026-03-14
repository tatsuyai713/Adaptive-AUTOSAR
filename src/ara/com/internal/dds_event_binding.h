/// @file src/ara/com/internal/dds_event_binding.h
/// @brief CycloneDDS-backed ProxyEventBinding and SkeletonEventBinding.
/// @details Provides raw-bytes DDS transport for ara::com events without
///          requiring IDL code generation.  A minimal DDS type descriptor is
///          defined for a struct { uint32_t size; uint8_t data[MAX]; } whose
///          maximum payload size is ARA_COM_DDS_MAX_PAYLOAD_SIZE bytes.
///
///          Both the proxy and skeleton sides run with a DDS Participant,
///          Publisher/Subscriber, and DataWriter/DataReader inside their own
///          DDS domain, identified by the EventBindingConfig service/instance IDs.
///
///          The proxy uses a background thread (WaitForData + Take loop) to
///          drain samples into an internal queue and notify the receive handler.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_INTERNAL_DDS_EVENT_BINDING_H
#define ARA_COM_INTERNAL_DDS_EVENT_BINDING_H

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include "./event_binding.h"
#include "../com_error_domain.h"

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <dds/dds.h>
#include <dds/ddsc/dds_opcodes.h>
#endif

namespace ara
{
    namespace com
    {
        namespace internal
        {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)

            /// @brief Maximum serialized payload size for one DDS raw-bytes event.
            ///        Payloads larger than this are rejected with kCommunicationStackError.
            static constexpr std::uint32_t ARA_COM_DDS_MAX_PAYLOAD_SIZE = 4096U;

            /// @brief In-memory layout of the raw-bytes DDS topic type.
            ///        - size:   number of valid bytes in the data array
            ///        - data[]: serialized ara::com event payload (padded with zeroes)
            struct AraComDdsRawEvent
            {
                std::uint32_t size;
                std::uint8_t  data[ARA_COM_DDS_MAX_PAYLOAD_SIZE];
            };

            /// @brief CDR type descriptor for AraComDdsRawEvent.
            ///
            /// Ops encoding (CycloneDDS dds_opcodes.h format):
            ///   [ADR, 4BY, 0, 0]  [offset_of_size]              — uint32_t field
            ///   [ADR, ARR, 1BY, 0] [offset_of_data] [alen]       — fixed byte array
            ///   [RTS]
            ///
            /// Aligned with CycloneDDS DDS_OP_* enumerations:
            ///   DDS_OP_ADR  = 0x01000000, DDS_OP_RTS    = 0x00000000
            ///   DDS_OP_TYPE_4BY  = (DDS_OP_VAL_4BY  << 16) = 0x00030000
            ///   DDS_OP_TYPE_ARR  = (DDS_OP_VAL_ARR  << 16) = 0x00080000
            ///   DDS_OP_SUBTYPE_1BY = (DDS_OP_VAL_1BY << 8) = 0x00000100
            extern const dds_topic_descriptor_t AraComDdsRawEvent_desc;

#endif // ARA_COM_USE_CYCLONEDDS

            /// @brief CycloneDDS-based proxy-side event binding.
            ///        Uses a background thread to drain samples from the DataReader.
            class DdsProxyEventBinding final : public ProxyEventBinding
            {
            private:
                EventBindingConfig mConfig;
                mutable std::mutex mMutex;
                SubscriptionState mState{SubscriptionState::kNotSubscribed};
                std::deque<std::vector<std::uint8_t>> mSampleQueue;
                std::size_t mMaxSampleCount{16U};
                std::function<void()> mReceiveHandler;
                SubscriptionStateChangeHandler mStateChangeHandler;

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t mParticipant{0};
                dds_entity_t mTopic{0};
                dds_entity_t mSubscriber{0};
                dds_entity_t mReader{0};
                dds_entity_t mWaitSet{0};
                dds_entity_t mReadCond{0};
#endif

                std::atomic<bool> mRunning{false};
                std::thread mPollThread;

                void pollLoop() noexcept;

            public:
                static std::string makeTopicName(
                    const EventBindingConfig &cfg) noexcept;

                explicit DdsProxyEventBinding(
                    EventBindingConfig config) noexcept;
                ~DdsProxyEventBinding() noexcept override;

                DdsProxyEventBinding(const DdsProxyEventBinding &) = delete;
                DdsProxyEventBinding &operator=(
                    const DdsProxyEventBinding &) = delete;

                core::Result<void> Subscribe(std::size_t maxSampleCount) override;
                void Unsubscribe() override;
                SubscriptionState GetSubscriptionState() const noexcept override;
                core::Result<std::size_t> GetNewSamples(
                    RawReceiveHandler handler,
                    std::size_t maxNumberOfSamples) override;
                void SetReceiveHandler(std::function<void()> handler) override;
                void UnsetReceiveHandler() override;
                std::size_t GetFreeSampleCount() const noexcept override;
                void SetSubscriptionStateChangeHandler(
                    SubscriptionStateChangeHandler handler) override;
                void UnsetSubscriptionStateChangeHandler() override;
            };

            /// @brief CycloneDDS-based skeleton-side event binding.
            class DdsSkeletonEventBinding final : public SkeletonEventBinding
            {
            private:
                EventBindingConfig mConfig;
                bool mOffered{false};

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t mParticipant{0};
                dds_entity_t mTopic{0};
                dds_entity_t mPublisher{0};
                dds_entity_t mWriter{0};
#endif

            public:
                explicit DdsSkeletonEventBinding(
                    EventBindingConfig config) noexcept;
                ~DdsSkeletonEventBinding() noexcept override;

                DdsSkeletonEventBinding(const DdsSkeletonEventBinding &) = delete;
                DdsSkeletonEventBinding &operator=(
                    const DdsSkeletonEventBinding &) = delete;

                core::Result<void> Offer() override;
                void StopOffer() override;
                core::Result<void> Send(
                    const std::vector<std::uint8_t> &payload) override;
                core::Result<void *> Allocate(std::size_t size) override;
                core::Result<void> SendAllocated(
                    void *data, std::size_t size) override;
            };
        }
    }
}

#endif // ARA_COM_INTERNAL_DDS_EVENT_BINDING_H
