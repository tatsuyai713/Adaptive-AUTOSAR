/// @file src/ara/core/error_code.cpp
/// @brief Implementation for error code.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./error_code.h"

namespace ara
{
    namespace core
    {
        std::string ErrorCode::Message() const noexcept
        {
            std::string _result(mDomain.Message(mValue));
            return _result;
        }

        void ErrorCode::ThrowAsException() const
        {
            mDomain.ThrowAsException(*this);
        }
    }
}