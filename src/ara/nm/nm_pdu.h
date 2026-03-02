/// @file src/ara/nm/nm_pdu.h
/// @brief NM PDU serialization/deserialization for Network Management.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.
///
/// Implements an AUTOSAR-style NM PDU with fixed header fields and
/// variable-length user data. Provides binary serialization/deserialization
/// and partial-networking control-bit support.

#ifndef NM_PDU_H
#define NM_PDU_H

#include <cstdint>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./nm_error_domain.h"

namespace ara
{
    namespace nm
    {
        /// @brief Control bit vector flags within an NM PDU.
        ///
        /// Bit positions follow AUTOSAR NM SWS conventions (simplified).
        enum class NmControlBit : std::uint8_t
        {
            kRepeatMessageRequest = 0x01U, ///< Sender requests repeat-message state.
            kNmCoordinatorSleep   = 0x02U, ///< Coordinator indicates sleep readiness.
            kActiveWakeup         = 0x04U, ///< Sender actively requested wakeup.
            kPartialNetwork       = 0x08U, ///< Partial networking information present.
            kPnEnabled            = 0x10U  ///< Partial networking enabled on sender.
        };

        /// @brief AUTOSAR-style NM Protocol Data Unit.
        ///
        /// Wire layout (big-endian):
        /// @code
        ///   [0]      Source Node ID          (1 byte)
        ///   [1]      Control Bit Vector      (1 byte)
        ///   [2..N]   User Data               (0..252 bytes, default 6)
        /// @endcode
        ///
        /// Maximum PDU size: 254 bytes (per AUTOSAR NM SWS).
        struct NmPdu
        {
            /// Node ID of the sender (0x00..0xFF).
            std::uint8_t SourceNodeId{0U};

            /// Control bit vector (bitwise OR of NmControlBit values).
            std::uint8_t ControlBitVector{0U};

            /// User-data bytes (typically 6 bytes for CAN-based NM).
            std::vector<std::uint8_t> UserData;

            /// Partial-Network cluster filter mask (used when kPartialNetwork
            /// is set). Up to 7 bytes per AUTOSAR PNC filter mask.
            std::vector<std::uint8_t> PnFilterMask;

            /// @brief Minimum serialized size (SourceNodeId + CBV).
            static constexpr std::size_t cHeaderSize{2U};

            /// @brief Maximum PDU length per AUTOSAR NM.
            static constexpr std::size_t cMaxPduLength{254U};

            // ----------------------------------------------------------------
            // Control-bit helpers
            // ----------------------------------------------------------------

            /// @brief Set a control bit flag.
            void SetControlBit(NmControlBit bit) noexcept;

            /// @brief Clear a control bit flag.
            void ClearControlBit(NmControlBit bit) noexcept;

            /// @brief Test whether a control bit flag is set.
            bool HasControlBit(NmControlBit bit) const noexcept;

            // ----------------------------------------------------------------
            // Serialization
            // ----------------------------------------------------------------

            /// @brief Serialize the PDU into a byte vector.
            /// @returns Serialized byte vector, or error if PDU is too large.
            core::Result<std::vector<std::uint8_t>> Serialize() const;

            /// @brief Deserialize an NM PDU from raw bytes.
            /// @param data       Raw byte buffer.
            /// @param userDataLen Expected user-data length; remaining bytes
            ///                   after header and user data are treated as PN
            ///                   filter mask (0 = auto-detect).
            /// @returns Parsed NmPdu, or error on invalid data.
            static core::Result<NmPdu> Deserialize(
                const std::vector<std::uint8_t> &data,
                std::size_t userDataLen = 6U);
        };

    } // namespace nm
} // namespace ara

#endif
