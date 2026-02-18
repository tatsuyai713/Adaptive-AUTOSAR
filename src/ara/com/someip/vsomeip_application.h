/// @file src/ara/com/someip/vsomeip_application.h
/// @brief Declarations for vsomeip application.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef VSOMEIP_APPLICATION_H
#define VSOMEIP_APPLICATION_H

#include <memory>

#ifdef ARA_COM_USE_VSOMEIP
#include <vsomeip/vsomeip.hpp>
#endif

namespace ara
{
    namespace com
    {
        namespace someip
        {
#ifdef ARA_COM_USE_VSOMEIP
            /// @brief Shared vsomeip application accessors for client/server roles.
            class VsomeipApplication
            {
            public:
                static std::shared_ptr<vsomeip::application> GetServerApplication();
                static std::shared_ptr<vsomeip::application> GetClientApplication();
                static void StopAll() noexcept;
                static void StopServerApplication() noexcept;
            };
#endif
        }
    }
}

#endif
