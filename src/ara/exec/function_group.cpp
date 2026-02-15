/// @file src/ara/exec/function_group.cpp
/// @brief Implementation for function group.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./function_group.h"

namespace ara
{
    namespace exec
    {
        FunctionGroup::FunctionGroup(std::string metaModelIdentifier) : mInstnaceSpecifier{metaModelIdentifier}
        {
        }

        core::Result<FunctionGroup> FunctionGroup::Create(
            std::string metaModelIdentifier)
        {
            core::Result<FunctionGroup> _result{FunctionGroup{metaModelIdentifier}};

            return _result;
        }

        FunctionGroup::FunctionGroup(FunctionGroup &&other) : mInstnaceSpecifier{std::move(other.mInstnaceSpecifier)}
        {
        }

        FunctionGroup &FunctionGroup::operator=(FunctionGroup &&other)
        {
            mInstnaceSpecifier = std::move(other.mInstnaceSpecifier);
            return *this;
        }

        const core::InstanceSpecifier &FunctionGroup::GetInstance() const noexcept
        {
            return mInstnaceSpecifier;
        }
    }
}