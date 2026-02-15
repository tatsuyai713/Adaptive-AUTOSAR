/// @file src/ara/diag/routing/nrc_exception.cpp
/// @brief Implementation for nrc exception.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./nrc_exception.h"

namespace ara
{
    namespace diag
    {
        namespace routing
        {
            NrcExecption::NrcExecption(uint8_t nrc) noexcept : mNrc{nrc}
            {
            }

            uint8_t NrcExecption::GetNrc() const noexcept
            {
                return mNrc;
            }
        }
    }
}