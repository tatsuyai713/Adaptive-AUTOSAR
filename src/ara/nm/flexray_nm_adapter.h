/// @file src/ara/nm/flexray_nm_adapter.h
/// @brief FlexRay NM transport adapter for bus-level NM PDU exchange.
/// @details Implements NmTransportAdapter for FlexRay communication cycles.

#ifndef FLEXRAY_NM_ADAPTER_H
#define FLEXRAY_NM_ADAPTER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "./nm_transport_adapter.h"

namespace ara
{
    namespace nm
    {
        /// @brief FlexRay communication cycle timing parameters.
        struct FlexRayTimingConfig
        {
            /// @brief Communication cycle period in microseconds.
            uint32_t CyclePeriodUs{5000};

            /// @brief Number of static slots per cycle.
            uint16_t StaticSlotCount{64};

            /// @brief Static slot duration in microseconds.
            uint16_t StaticSlotDurationUs{50};

            /// @brief Payload length per static slot in bytes (max 254).
            uint16_t StaticPayloadBytes{32};
        };

        /// @brief FlexRay NM slot assignment for a channel.
        struct FlexRayNmSlot
        {
            /// @brief Static slot index used for NM voting (0-based).
            uint16_t NmVotingSlot{0};

            /// @brief Static slot index used for NM data (0-based).
            uint16_t NmDataSlot{1};
        };

        /// @brief Configuration for the FlexRay NM adapter.
        struct FlexRayNmConfig
        {
            /// @brief FlexRay cluster name / controller interface.
            std::string ClusterName{"flexray0"};

            /// @brief Communication cycle timing.
            FlexRayTimingConfig Timing;

            /// @brief Default NM slot assignment.
            FlexRayNmSlot DefaultSlots;

            /// @brief Poll timeout in milliseconds.
            uint32_t PollTimeoutMs{10};
        };

        /// @brief FlexRay NM transport adapter.
        /// @details Simulates FlexRay NM communication using slot-based framing.
        ///          In production, this would bind to a FlexRay controller driver.
        class FlexRayNmAdapter : public NmTransportAdapter
        {
        public:
            explicit FlexRayNmAdapter(FlexRayNmConfig config);
            ~FlexRayNmAdapter() override;

            core::Result<void> SendNmPdu(
                const std::string &channelName,
                const std::vector<uint8_t> &pduData) override;

            core::Result<void> RegisterReceiveHandler(
                NmPduHandler handler) override;

            core::Result<void> Start() override;
            void Stop() override;

            /// @brief Map a channel to specific FlexRay NM slots.
            void MapChannel(const std::string &channelName,
                            FlexRayNmSlot slots);

            /// @brief Check if the adapter is running.
            bool IsRunning() const noexcept;

            /// @brief Get the configured timing parameters.
            const FlexRayTimingConfig &GetTiming() const noexcept;

            /// @brief Get sent PDU count.
            uint64_t GetSentCount() const noexcept;

            /// @brief Get received PDU count.
            uint64_t GetReceivedCount() const noexcept;

            /// @brief Inject a received NM PDU (for testing without hardware).
            void InjectReceivedPdu(const std::string &channelName,
                                   const std::vector<uint8_t> &pduData);

        private:
            FlexRayNmConfig mConfig;
            std::atomic<bool> mRunning{false};
            std::thread mCycleThread;
            mutable std::mutex mMutex;

            NmPduHandler mHandler;
            std::unordered_map<std::string, FlexRayNmSlot> mChannelSlots;

            /// @brief Pending TX queue: channel → PDU data.
            std::unordered_map<std::string, std::vector<uint8_t>> mPendingTx;

            std::atomic<uint64_t> mSentCount{0};
            std::atomic<uint64_t> mReceivedCount{0};

            /// @brief Unix domain socket fd for inter-process bus simulation (-1 = disabled).
            int mSocket{-1};
            /// @brief Bound socket path (empty if socket not created).
            std::string mSocketPath;

            void CycleLoop();
        };
    }
}

#endif
