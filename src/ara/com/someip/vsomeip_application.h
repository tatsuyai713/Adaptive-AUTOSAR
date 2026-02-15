#ifndef VSOMEIP_APPLICATION_H
#define VSOMEIP_APPLICATION_H

#include <memory>
#include <vsomeip/vsomeip.hpp>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief Shared vsomeip application accessors for client/server roles.
            class VsomeipApplication
            {
            public:
                static std::shared_ptr<vsomeip::application> GetServerApplication();
                static std::shared_ptr<vsomeip::application> GetClientApplication();
                static void StopAll() noexcept;
            };
        }
    }
}

#endif
