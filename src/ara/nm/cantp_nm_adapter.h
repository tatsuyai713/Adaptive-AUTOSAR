/// @file src/ara/nm/cantp_nm_adapter.h
/// @brief CAN-TP NM transport adapter for bus-level NM PDU exchange.
/// @details Implements NmTransportAdapter over Linux SocketCAN (PF_CAN)
///          or QNX CAN resource-manager (/dev/can<N>/).

#ifndef CANTP_NM_ADAPTER_H
#define CANTP_NM_ADAPTER_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "./nm_transport_adapter.h"

namespace ara
{
    namespace nm
    {
        /// @brief CAN arbitration ID configuration for NM frames.
        struct CanNmConfig
        {
            /// @brief CAN interface name (e.g. "vcan0", "can0").
            std::string InterfaceName{"vcan0"};

            /// @brief Base CAN arbitration ID for NM frames (11-bit or 29-bit).
            uint32_t BaseCanId{0x600};

            /// @brief Whether to use 29-bit extended CAN IDs.
            bool UseExtendedId{false};

            /// @brief Maximum payload size per CAN frame (8 for classic CAN, 64 for CAN-FD).
            uint8_t MaxPayload{8};

            /// @brief Poll timeout in milliseconds for the receive thread.
            uint32_t PollTimeoutMs{50};
        };

        /// @brief CAN-TP NM transport adapter using SocketCAN (Linux) or
    ///        CAN resource-manager (QNX).
        /// @details Sends/receives NM PDUs as CAN frames. Each channel maps to
        ///          a CAN arbitration ID offset from BaseCanId.
        class CanTpNmAdapter : public NmTransportAdapter
        {
        public:
            explicit CanTpNmAdapter(CanNmConfig config);
            ~CanTpNmAdapter() override;

            core::Result<void> SendNmPdu(
                const std::string &channelName,
                const std::vector<uint8_t> &pduData) override;

            core::Result<void> RegisterReceiveHandler(
                NmPduHandler handler) override;

            core::Result<void> Start() override;
            void Stop() override;

            /// @brief Register a channel with a CAN-ID offset from BaseCanId.
            void MapChannel(const std::string &channelName, uint32_t canIdOffset);

            /// @brief Check whether the adapter is currently running.
            bool IsRunning() const noexcept;

            /// @brief Get the number of PDUs sent since Start().
            uint64_t GetSentCount() const noexcept;

            /// @brief Get the number of PDUs received since Start().
            uint64_t GetReceivedCount() const noexcept;

        private:
            CanNmConfig mConfig;
            int mSocket{-1};
#if defined(__QNX__)
            int mTxFd{-1}; ///< QNX: separate fd for /dev/can<N>/tx0
#endif
            std::atomic<bool> mRunning{false};
            std::thread mReceiveThread;
            mutable std::mutex mMutex;

            NmPduHandler mHandler;
            std::unordered_map<std::string, uint32_t> mChannelToCanId;
            std::unordered_map<uint32_t, std::string> mCanIdToChannel;

            std::atomic<uint64_t> mSentCount{0};
            std::atomic<uint64_t> mReceivedCount{0};

            void ReceiveLoop();
            std::string FindChannelByCanId(uint32_t canId) const;
        };
    }
}

#endif
