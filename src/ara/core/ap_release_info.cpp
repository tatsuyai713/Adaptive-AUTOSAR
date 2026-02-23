/// @file src/ara/core/ap_release_info.cpp
/// @brief Out-of-class definitions for C++14 linkage of release constants.

#include "ap_release_info.h"

namespace ara
{
    namespace core
    {
        constexpr const char *ApReleaseInfo::cReleaseString;
        constexpr std::uint16_t ApReleaseInfo::cReleaseYear;
        constexpr std::uint8_t ApReleaseInfo::cReleaseMonth;
        constexpr std::uint16_t ApReleaseInfo::cReleaseCompact;
    }
}
