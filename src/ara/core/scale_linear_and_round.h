/// @file src/ara/core/scale_linear_and_round.h
/// @brief ara::core::ScaleLinearAndRound — AUTOSAR AP SWS_CORE_04110.
/// @details Linear scaling with rounding: result = round((value * factor) + offset).

#ifndef ARA_CORE_SCALE_LINEAR_AND_ROUND_H
#define ARA_CORE_SCALE_LINEAR_AND_ROUND_H

#include <cmath>
#include <type_traits>

namespace ara
{
    namespace core
    {
        /// @brief Linear scaling with rounding (SWS_CORE_04110).
        /// @tparam T  Arithmetic output type
        /// @tparam U  Arithmetic input type
        /// @param value  The raw value to scale
        /// @param factor Multiplicative scaling factor
        /// @param offset Additive offset applied after scaling
        /// @return round(value * factor + offset), cast to T
        template <typename T, typename U>
        T ScaleLinearAndRound(U value, double factor, double offset)
        {
            static_assert(std::is_arithmetic<T>::value, "T must be arithmetic");
            static_assert(std::is_arithmetic<U>::value, "U must be arithmetic");
            return static_cast<T>(std::round(static_cast<double>(value) * factor + offset));
        }

    } // namespace core
} // namespace ara

#endif // ARA_CORE_SCALE_LINEAR_AND_ROUND_H
