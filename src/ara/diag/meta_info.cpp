/// @file src/ara/diag/meta_info.cpp
/// @brief Implementation for meta info.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <stdexcept>
#include "./meta_info.h"

namespace ara
{
    namespace diag
    {
        MetaInfo::MetaInfo(Context context) : mContext{context}
        {
        }

        core::Optional<std::string> MetaInfo::GetValue(std::string key)
        {
            try
            {
                core::Optional<std::string> _result{mValues.at(key)};
                return _result;
            }
            catch (std::out_of_range)
            {
                core::Optional<std::string> _result;
                return _result;
            }
        }

        void MetaInfo::SetValue(std::string key, std::string value)
        {
            std::pair<std::string, std::string> _pair(key, value);
            mValues.insert(_pair);
        }

        Context MetaInfo::GetContext() const noexcept
        {
            return mContext;
        }
    }
}