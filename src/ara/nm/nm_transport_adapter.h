/// @file src/ara/nm/nm_transport_adapter.h
/// @brief Abstract NM transport adapter interface.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef NM_TRANSPORT_ADAPTER_H
#define NM_TRANSPORT_ADAPTER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./nm_error_domain.h"

namespace ara
{
    namespace nm
    {
        /// @brief Abstract interface for NM PDU transport.
        class NmTransportAdapter
        {
        public:
            virtual ~NmTransportAdapter() = default;

            /// @brief Callback type for received NM PDUs.
            using NmPduHandler = std::function<void(
                const std::string &channelName,
                const std::vector<std::uint8_t> &pduData)>;

            /// @brief Send an NM PDU on the specified channel.
            virtual core::Result<void> SendNmPdu(
                const std::string &channelName,
                const std::vector<std::uint8_t> &pduData) = 0;

            /// @brief Register a handler for received NM PDUs.
            virtual core::Result<void> RegisterReceiveHandler(
                NmPduHandler handler) = 0;

            /// @brief Start the transport adapter.
            virtual core::Result<void> Start() = 0;

            /// @brief Stop the transport adapter.
            virtual void Stop() = 0;
        };
    }
}

#endif
