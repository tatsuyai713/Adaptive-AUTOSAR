/// @file src/ara/core/error_code.h
/// @brief Declarations for error code.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ERROR_CODE_H
#define ERROR_CODE_H

#include <string>
#include "./error_domain.h"

namespace ara
{
    namespace core
    {
        /// @brief A wrapper around the raw error code in a specific ErrorDomain (SWS_CORE_00501)
        class ErrorCode final
        {
        private:
            ErrorDomain::CodeType mValue;
            ErrorDomain::SupportDataType mSupportData;
            const ErrorDomain& mDomain;

        public:
            /// @brief Constructor (2-arg convenience)
            /// @param value Error code value
            /// @param domain Error code domain
            constexpr ErrorCode(
                ErrorDomain::CodeType value,
                const ErrorDomain &domain) noexcept
                : mValue{value}, mSupportData{0}, mDomain{domain}
            {
            }

            /// @brief Constructor with support data (SWS_CORE_00512)
            /// @param value Error code value
            /// @param domain Error code domain
            /// @param data Vendor-specific supplementary error data
            constexpr ErrorCode(
                ErrorDomain::CodeType value,
                const ErrorDomain &domain,
                ErrorDomain::SupportDataType data) noexcept
                : mValue{value}, mSupportData{data}, mDomain{domain}
            {
            }

            ErrorCode() = delete;
            /// @brief Destructor.
            ~ErrorCode() noexcept = default;

            /// @brief Get error code value (SWS_CORE_00514)
            /// @returns Raw error code value
            constexpr ErrorDomain::CodeType Value() const noexcept
            {
                return mValue;
            }

            /// @brief Get error code domain (SWS_CORE_00515)
            /// @returns Error domain which the error code belongs to
            constexpr ErrorDomain const &Domain() const noexcept
            {
                return mDomain;
            }

            /// @brief Get supplementary error data (SWS_CORE_00516)
            /// @returns Vendor-specific support data
            constexpr ErrorDomain::SupportDataType SupportData() const noexcept
            {
                return mSupportData;
            }

            /// @brief Get error message (SWS_CORE_00518)
            /// @returns Error code corresponding message in the defined domain
            std::string Message() const noexcept;

            /// @brief Throw the error as an exception (SWS_CORE_00519)
            /// @note Delegates to Domain().ThrowAsException(*this)
            void ThrowAsException() const;

            /// @brief Equality comparison for domain and value.
            constexpr bool operator==(const ErrorCode &other) const noexcept
            {
                return mDomain == other.mDomain && mValue == other.mValue;
            }

            /// @brief Inequality comparison for domain and value.
            constexpr bool operator!=(const ErrorCode &other) const noexcept
            {
                return mDomain != other.mDomain || mValue != other.mValue;
            }
        };
    }
}

#endif
