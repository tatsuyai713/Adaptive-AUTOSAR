/// @file src/ara/core/core_error_domain.h
/// @brief ara::core CoreErrorDomain and standard core exception hierarchy.
/// @details Implements the standard AUTOSAR Adaptive Platform core error domain
///          (SWS_CORE_10400) with standard error codes and exception types.
///
///          Standard error codes (SWS_CORE_10901):
///          - kInvalidArgument:           1 — an invalid argument was passed
///          - kInvalidMetaModelShortname: 2 — shortname path element is syntactically wrong
///          - kInvalidMetaModelPath:      3 — path does not conform to meta-model rules
///
///          Domain ID: 0x8000000000000014 (AUTOSAR-assigned)
///
///          Exception hierarchy (SWS_CORE_10906-10910):
///          - Exception (base, wraps ErrorCode)
///            - CoreException
///              - InvalidArgumentException
///              - InvalidMetaModelShortnameException
///              - InvalidMetaModelPathException
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_CORE_ERROR_DOMAIN_H
#define ARA_CORE_CORE_ERROR_DOMAIN_H

#include <stdexcept>
#include "./error_domain.h"
#include "./error_code.h"

namespace ara
{
    namespace core
    {
        // -----------------------------------------------------------------------
        // CoreErrc — enumeration of standard core error codes
        // -----------------------------------------------------------------------
        /// @brief Error codes belonging to CoreErrorDomain (SWS_CORE_10901).
        enum class CoreErrc : ErrorDomain::CodeType
        {
            kInvalidArgument           = 1, ///< Invalid argument passed to an API
            kInvalidMetaModelShortname = 2, ///< Shortname path element syntactically wrong
            kInvalidMetaModelPath      = 3, ///< Path violates meta-model path rules
        };

        // -----------------------------------------------------------------------
        // Exception hierarchy (SWS_CORE_10906-10910)
        // -----------------------------------------------------------------------
        /// @brief Base class for all ara::core exceptions (SWS_CORE_10906).
        class Exception : public std::exception
        {
        public:
            explicit Exception(ErrorCode ec) noexcept
                : mErrorCode{ec}
            {
            }

            const char *what() const noexcept override
            {
                return mErrorCode.Domain().Message(mErrorCode.Value());
            }

            /// @brief Get the embedded ErrorCode.
            const ErrorCode &Error() const noexcept
            {
                return mErrorCode;
            }

        protected:
            ErrorCode mErrorCode;
        };

        /// @brief Exception type for CoreErrorDomain errors (SWS_CORE_10907).
        class CoreException : public Exception
        {
        public:
            explicit CoreException(ErrorCode ec) noexcept : Exception{ec} {}
        };

        /// @brief Exception for kInvalidArgument.
        class InvalidArgumentException : public CoreException
        {
        public:
            explicit InvalidArgumentException(ErrorCode ec) noexcept : CoreException{ec} {}
        };

        /// @brief Exception for kInvalidMetaModelShortname.
        class InvalidMetaModelShortnameException : public CoreException
        {
        public:
            explicit InvalidMetaModelShortnameException(ErrorCode ec) noexcept
                : CoreException{ec} {}
        };

        /// @brief Exception for kInvalidMetaModelPath.
        class InvalidMetaModelPathException : public CoreException
        {
        public:
            explicit InvalidMetaModelPathException(ErrorCode ec) noexcept
                : CoreException{ec} {}
        };

        // -----------------------------------------------------------------------
        // CoreErrorDomain
        // -----------------------------------------------------------------------
        /// @brief The error domain for ara::core errors (SWS_CORE_10400).
        /// @details Domain ID 0x8000000000000014 per AUTOSAR AP SWS_CORE_10951.
        class CoreErrorDomain final : public ErrorDomain
        {
        public:
            /// @brief AUTOSAR-assigned domain ID for CoreErrorDomain.
            static constexpr IdType kId{0x8000000000000014ULL};

            constexpr CoreErrorDomain() noexcept : ErrorDomain{kId} {}

            const char *Name() const noexcept override
            {
                return "Core";
            }

            const char *Message(CodeType errorCode) const noexcept override
            {
                switch (static_cast<CoreErrc>(errorCode))
                {
                case CoreErrc::kInvalidArgument:
                    return "Invalid argument";
                case CoreErrc::kInvalidMetaModelShortname:
                    return "Invalid meta-model shortname path element";
                case CoreErrc::kInvalidMetaModelPath:
                    return "Invalid meta-model path";
                default:
                    return "Unknown core error";
                }
            }

            /// @brief Throw the appropriate exception type for this error code.
            void ThrowAsException(const ErrorCode &ec) const
            {
                switch (static_cast<CoreErrc>(ec.Value()))
                {
                case CoreErrc::kInvalidArgument:
                    throw InvalidArgumentException{ec};
                case CoreErrc::kInvalidMetaModelShortname:
                    throw InvalidMetaModelShortnameException{ec};
                case CoreErrc::kInvalidMetaModelPath:
                    throw InvalidMetaModelPathException{ec};
                default:
                    throw CoreException{ec};
                }
            }
        };

        // -----------------------------------------------------------------------
        // Domain accessor and MakeErrorCode
        // -----------------------------------------------------------------------
        /// @brief Get a reference to the singleton CoreErrorDomain.
        inline const CoreErrorDomain &GetCoreErrorDomain() noexcept
        {
            static const CoreErrorDomain sDomain{};
            return sDomain;
        }

        /// @brief Create an ErrorCode for a CoreErrc value (SWS_CORE_10952).
        inline ErrorCode MakeErrorCode(CoreErrc code) noexcept
        {
            return ErrorCode{static_cast<ErrorDomain::CodeType>(code),
                             GetCoreErrorDomain()};
        }

    } // namespace core
} // namespace ara

#endif // ARA_CORE_CORE_ERROR_DOMAIN_H
