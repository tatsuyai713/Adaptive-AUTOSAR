/// @file src/ara/sm/power_mode_manager.cpp
/// @brief Implementation of PowerModeManager.

#include "./power_mode_manager.h"

namespace ara
{
    namespace sm
    {
        core::Result<PowerModeRespMsg> PowerModeManager::RequestPowerMode(PowerModeMsg mode)
        {
            mCurrentMode = mode;
            if (mCallback)
            {
                mCallback(mode);
            }
            return core::Result<PowerModeRespMsg>::FromValue(PowerModeRespMsg::kDone);
        }

        void PowerModeManager::SetPowerModeChangeCallback(PowerModeCallback callback)
        {
            mCallback = std::move(callback);
        }
    }
}
