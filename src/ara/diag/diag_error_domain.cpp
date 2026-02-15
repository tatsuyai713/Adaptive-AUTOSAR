/// @file src/ara/diag/diag_error_domain.cpp
/// @brief Implementation for diag error domain.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        const ara::core::ErrorDomain::IdType DiagErrorDomain::cId;

        const char *DiagErrorDomain::Name() const noexcept
        {
            return cName;
        }

        const char *DiagErrorDomain::Message(ara::core::ErrorDomain::CodeType errorCode) const noexcept
        {
            auto diagErrc{static_cast<ara::diag::DiagErrc>(errorCode)};

            switch (diagErrc)
            {
            case ara::diag::DiagErrc::kAlreadyOffered:
                return "Already offered service";
            case ara::diag::DiagErrc::kConfigurationMismatch:
                return "Configuration misalignment with DEXT";
            case ara::diag::DiagErrc::kDebouncingConfigurationInconsistent:
                return "Invalid monitor debouncing configuration";
            case ara::diag::DiagErrc::kReportIgnored:
                return "Disabled control DTC setting";
            case ara::diag::DiagErrc::kInvalidArgument:
                return "Invalid passed argument from caller";
            case ara::diag::DiagErrc::kNotOffered:
                return "Request from a not offered service";
            case ara::diag::DiagErrc::kGenericError:
                return "General error occurrance";
            case ara::diag::DiagErrc::kNoSuchDTC:
                return "Invalid DTC number";
            case ara::diag::DiagErrc::kBusy:
                return "Busy interface call";
            case ara::diag::DiagErrc::kFailed:
                return "Failed process";
            case ara::diag::DiagErrc::kMemoryError:
                return "Memory error occurrance";
            case ara::diag::DiagErrc::kWrongDtc:
                return "Incorrect passed DTC number";
            case ara::diag::DiagErrc::kRejected:
                return "Request rejection";
            case ara::diag::DiagErrc::kResetTypeNotSupported:
                return "Unsupported reset type by the Diagnostic Address instance";
            case ara::diag::DiagErrc::kRequestFailed:
                return "Failed diagnostic request process";
            default:
                return "Not supported";
            }
        }

        ara::core::ErrorDomain *DiagErrorDomain::GetDiagDomain()
        {
            static DiagErrorDomain cDomain;
            return &cDomain;
        }

        ara::core::ErrorCode DiagErrorDomain::MakeErrorCode(DiagErrc code) noexcept
        {
            auto _codeType{static_cast<ara::core::ErrorDomain::CodeType>(code)};
            static DiagErrorDomain cDomain;
            ara::core::ErrorCode _result(_codeType, cDomain);

            return _result;
        }

        const ara::core::ErrorDomain &GetDiagErrorDomain() noexcept
        {
            return *(DiagErrorDomain::GetDiagDomain());
        }

        ara::core::ErrorCode MakeErrorCode(DiagErrc code) noexcept
        {
            auto _codeType{static_cast<ara::core::ErrorDomain::CodeType>(code)};
            return ara::core::ErrorCode(_codeType, GetDiagErrorDomain());
        }
    }
}
