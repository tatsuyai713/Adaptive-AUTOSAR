/// @file src/ara/com/ap_release_info.h
/// @brief AUTOSAR AP release profile metadata for ara::com.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_AP_RELEASE_INFO_H
#define ARA_COM_AP_RELEASE_INFO_H

#include <cstdint>
#include "../core/ap_release_info.h"

#if !defined(ARA_COM_AP_RELEASE_STRING) && defined(AUTOSAR_AP_RELEASE_STRING)
#define ARA_COM_AP_RELEASE_STRING AUTOSAR_AP_RELEASE_STRING
#endif
#ifndef ARA_COM_AP_RELEASE_STRING
#define ARA_COM_AP_RELEASE_STRING "R24-11"
#endif

#if !defined(ARA_COM_AP_RELEASE_YEAR) && defined(AUTOSAR_AP_RELEASE_YEAR)
#define ARA_COM_AP_RELEASE_YEAR AUTOSAR_AP_RELEASE_YEAR
#endif
#ifndef ARA_COM_AP_RELEASE_YEAR
#define ARA_COM_AP_RELEASE_YEAR 24
#endif

#if !defined(ARA_COM_AP_RELEASE_MONTH) && defined(AUTOSAR_AP_RELEASE_MONTH)
#define ARA_COM_AP_RELEASE_MONTH AUTOSAR_AP_RELEASE_MONTH
#endif
#ifndef ARA_COM_AP_RELEASE_MONTH
#define ARA_COM_AP_RELEASE_MONTH 11
#endif

namespace ara
{
    namespace com
    {
        /// @brief Compile-time AP release profile selected for ara::com.
        struct ApReleaseInfo final
        {
            static constexpr const char *cReleaseString{
                ARA_COM_AP_RELEASE_STRING};
            static constexpr std::uint16_t cReleaseYear{
                static_cast<std::uint16_t>(ARA_COM_AP_RELEASE_YEAR)};
            static constexpr std::uint8_t cReleaseMonth{
                static_cast<std::uint8_t>(ARA_COM_AP_RELEASE_MONTH)};
            static constexpr std::uint16_t cReleaseCompact{
                static_cast<std::uint16_t>(
                    (cReleaseYear * 100U) + cReleaseMonth)};

            /// @brief Checks if selected release is greater-or-equal to `Ryy-mm`.
            static constexpr bool IsAtLeast(
                std::uint16_t year,
                std::uint8_t month) noexcept
            {
                return ara::core::ApReleaseInfo::IsAtLeast(
                    year,
                    month);
            }
        };
    }
}

#endif
