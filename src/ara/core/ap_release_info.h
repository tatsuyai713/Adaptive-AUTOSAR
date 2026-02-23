/// @file src/ara/core/ap_release_info.h
/// @brief AUTOSAR AP release profile metadata shared by all ara::* modules.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_AP_RELEASE_INFO_H
#define ARA_CORE_AP_RELEASE_INFO_H

#include <cstdint>

#ifndef AUTOSAR_AP_RELEASE_STRING
#define AUTOSAR_AP_RELEASE_STRING "R24-11"
#endif

#ifndef AUTOSAR_AP_RELEASE_YEAR
#define AUTOSAR_AP_RELEASE_YEAR 24
#endif

#ifndef AUTOSAR_AP_RELEASE_MONTH
#define AUTOSAR_AP_RELEASE_MONTH 11
#endif

namespace ara
{
    namespace core
    {
        /// @brief Compile-time AP release profile selected for this repository.
        struct ApReleaseInfo final
        {
            static constexpr const char *cReleaseString{
                AUTOSAR_AP_RELEASE_STRING};
            static constexpr std::uint16_t cReleaseYear{
                static_cast<std::uint16_t>(AUTOSAR_AP_RELEASE_YEAR)};
            static constexpr std::uint8_t cReleaseMonth{
                static_cast<std::uint8_t>(AUTOSAR_AP_RELEASE_MONTH)};
            static constexpr std::uint16_t cReleaseCompact{
                static_cast<std::uint16_t>(
                    (cReleaseYear * 100U) + cReleaseMonth)};

            /// @brief Checks if selected release is greater-or-equal to `Ryy-mm`.
            static constexpr bool IsAtLeast(
                std::uint16_t year,
                std::uint8_t month) noexcept
            {
                return (cReleaseYear > year) ||
                       ((cReleaseYear == year) &&
                        (cReleaseMonth >= month));
            }
        };
    }
}

#endif
