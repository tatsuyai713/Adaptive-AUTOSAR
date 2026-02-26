/// @file src/ara/com/zerocopy/zero_copy.h
/// @brief Declarations for zero copy.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_ZEROCOPY_ZERO_COPY_H
#define ARA_COM_ZEROCOPY_ZERO_COPY_H

#include <chrono>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../../core/result.h"

namespace ara
{
    namespace com
    {
        namespace zerocopy
        {
            /// @brief Logical channel identifier for zero-copy communication.
            struct ChannelDescriptor
            {
                std::string Service;
                std::string Instance;
                std::string Event;
            };

            /// @brief Move-only RAII wrapper around a loaned zero-copy publisher buffer.
            class LoanedSample final
            {
            private:
                /// @brief Opaque implementation holder for transport-specific resources.
                class Impl;
                std::unique_ptr<Impl> mImpl;

                explicit LoanedSample(std::unique_ptr<Impl> impl) noexcept;
                friend class ZeroCopyPublisher;

            public:
                LoanedSample() noexcept;
                ~LoanedSample() noexcept;

                LoanedSample(const LoanedSample &) = delete;
                LoanedSample &operator=(const LoanedSample &) = delete;

                LoanedSample(LoanedSample &&other) noexcept;
                LoanedSample &operator=(LoanedSample &&other) noexcept;

                bool IsValid() const noexcept;
                std::uint8_t *Data() noexcept;
                const std::uint8_t *Data() const noexcept;
                std::size_t Size() const noexcept;
            };

            /// @brief Move-only RAII wrapper around a received zero-copy subscriber buffer.
            class ReceivedSample final
            {
            private:
                /// @brief Opaque implementation holder for transport-specific resources.
                class Impl;
                std::unique_ptr<Impl> mImpl;

                explicit ReceivedSample(std::unique_ptr<Impl> impl) noexcept;
                friend class ZeroCopySubscriber;

            public:
                ReceivedSample() noexcept;
                ~ReceivedSample() noexcept;

                ReceivedSample(const ReceivedSample &) = delete;
                ReceivedSample &operator=(const ReceivedSample &) = delete;

                ReceivedSample(ReceivedSample &&other) noexcept;
                ReceivedSample &operator=(ReceivedSample &&other) noexcept;

                bool IsValid() const noexcept;
                const std::uint8_t *Data() const noexcept;
                std::size_t Size() const noexcept;
            };

            /// @brief Zero-copy publisher wrapper for ara::com payloads.
            ///        Current runtime binding is iceoryx.
            class ZeroCopyPublisher final
            {
            private:
                /// @brief Opaque implementation holder for transport-specific resources.
                class Impl;
                std::unique_ptr<Impl> mImpl;

            public:
                ZeroCopyPublisher(
                    ChannelDescriptor channel,
                    std::string runtimeName = "adaptive_autosar_ara_com",
                    std::uint64_t historyCapacity = 0U);
                ~ZeroCopyPublisher() noexcept;

                ZeroCopyPublisher(const ZeroCopyPublisher &) = delete;
                ZeroCopyPublisher &operator=(const ZeroCopyPublisher &) = delete;

                ZeroCopyPublisher(ZeroCopyPublisher &&other) noexcept;
                ZeroCopyPublisher &operator=(ZeroCopyPublisher &&other) noexcept;

                bool IsBindingActive() const noexcept;

                core::Result<void> Loan(
                    std::size_t payloadSize,
                    LoanedSample &sample,
                    std::size_t payloadAlignment = 1U) noexcept;

                core::Result<void> Publish(LoanedSample &&sample) noexcept;

                core::Result<void> PublishCopy(
                    const std::vector<std::uint8_t> &payload) noexcept;

                bool HasSubscribers() const noexcept;
            };

            /// @brief Zero-copy subscriber wrapper for ara::com payloads.
            ///        Current runtime binding is iceoryx.
            class ZeroCopySubscriber final
            {
            private:
                /// @brief Opaque implementation holder for transport-specific resources.
                class Impl;
                std::unique_ptr<Impl> mImpl;

            public:
                ZeroCopySubscriber(
                    ChannelDescriptor channel,
                    std::string runtimeName = "adaptive_autosar_ara_com",
                    std::uint64_t queueCapacity = 256U,
                    std::uint64_t historyRequest = 0U);
                ~ZeroCopySubscriber() noexcept;

                ZeroCopySubscriber(const ZeroCopySubscriber &) = delete;
                ZeroCopySubscriber &operator=(const ZeroCopySubscriber &) = delete;

                ZeroCopySubscriber(ZeroCopySubscriber &&other) noexcept;
                ZeroCopySubscriber &operator=(ZeroCopySubscriber &&other) noexcept;

                bool IsBindingActive() const noexcept;

                /// @brief Try to receive one sample without copying.
                /// @param[out] sample Received sample wrapper (overwritten on success)
                /// @returns Result with 'true' when a sample is available, 'false' when no data is available
                core::Result<bool> TryTake(ReceivedSample &sample) noexcept;

                /// @brief Block until new data is available or the timeout expires.
                /// @details Uses the iceoryx WaitSet mechanism — no busy-wait or sleep.
                /// @param timeout Maximum time to wait.
                /// @returns true when data is available, false on timeout or error.
                bool WaitForData(std::chrono::milliseconds timeout) noexcept;
            };
        }
    }
}

#endif
