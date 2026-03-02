/// @file src/ara/nm/nm_pdu.cpp
/// @brief Implementation for NM PDU serialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./nm_pdu.h"

namespace ara
{
    namespace nm
    {
        // -----------------------------------------------------------------------
        // Control-bit helpers
        // -----------------------------------------------------------------------

        void NmPdu::SetControlBit(NmControlBit bit) noexcept
        {
            ControlBitVector |= static_cast<std::uint8_t>(bit);
        }

        void NmPdu::ClearControlBit(NmControlBit bit) noexcept
        {
            ControlBitVector &= static_cast<std::uint8_t>(
                ~static_cast<std::uint8_t>(bit));
        }

        bool NmPdu::HasControlBit(NmControlBit bit) const noexcept
        {
            return (ControlBitVector &
                    static_cast<std::uint8_t>(bit)) != 0U;
        }

        // -----------------------------------------------------------------------
        // Serialization
        // -----------------------------------------------------------------------

        core::Result<std::vector<std::uint8_t>> NmPdu::Serialize() const
        {
            const std::size_t totalSize =
                cHeaderSize + UserData.size() + PnFilterMask.size();

            if (totalSize > cMaxPduLength)
            {
                return core::Result<std::vector<std::uint8_t>>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

            std::vector<std::uint8_t> buf;
            buf.reserve(totalSize);

            // Header
            buf.push_back(SourceNodeId);
            buf.push_back(ControlBitVector);

            // User data
            buf.insert(buf.end(), UserData.begin(), UserData.end());

            // PN filter mask (appended after user data when kPartialNetwork is set)
            if (!PnFilterMask.empty())
            {
                buf.insert(buf.end(), PnFilterMask.begin(), PnFilterMask.end());
            }

            return core::Result<std::vector<std::uint8_t>>::FromValue(
                std::move(buf));
        }

        core::Result<NmPdu> NmPdu::Deserialize(
            const std::vector<std::uint8_t> &data,
            std::size_t userDataLen)
        {
            if (data.size() < cHeaderSize)
            {
                return core::Result<NmPdu>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

            if (data.size() > cMaxPduLength)
            {
                return core::Result<NmPdu>::FromError(
                    MakeErrorCode(NmErrc::kInvalidState));
            }

            NmPdu pdu;
            pdu.SourceNodeId = data[0];
            pdu.ControlBitVector = data[1];

            const std::size_t payloadSize = data.size() - cHeaderSize;

            // Determine how much is user data vs PN filter mask.
            const std::size_t actualUserLen =
                (userDataLen <= payloadSize) ? userDataLen : payloadSize;

            if (actualUserLen > 0U)
            {
                pdu.UserData.assign(
                    data.begin() + static_cast<std::ptrdiff_t>(cHeaderSize),
                    data.begin() + static_cast<std::ptrdiff_t>(cHeaderSize + actualUserLen));
            }

            // Remaining bytes are PN filter mask (if any and if PN bit set)
            if (payloadSize > actualUserLen)
            {
                const auto pnStart =
                    data.begin() +
                    static_cast<std::ptrdiff_t>(cHeaderSize + actualUserLen);
                pdu.PnFilterMask.assign(pnStart, data.end());
            }

            return core::Result<NmPdu>::FromValue(std::move(pdu));
        }

    } // namespace nm
} // namespace ara
