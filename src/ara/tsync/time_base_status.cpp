/// @file src/ara/tsync/time_base_status.cpp
/// @brief Implementation of UserTimeBaseProvider.

#include "./time_base_status.h"
#include "./time_sync_client.h"
#include "./tsync_error_domain.h"

namespace ara
{
    namespace tsync
    {
        UserTimeBaseProvider::UserTimeBaseProvider(
            const std::string &name,
            TimeSourceFunction source)
            : mName{name}, mSource{std::move(source)}
        {
        }

        core::Result<void> UserTimeBaseProvider::UpdateTimeBase(TimeSyncClient &client)
        {
            if (!mSource)
                return core::Result<void>::FromError(
                    MakeErrorCode(TsyncErrc::kProviderUnavailable));

            auto ns = mSource();
            auto tp = std::chrono::steady_clock::time_point(
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(ns));
            return client.UpdateReferenceTime(
                std::chrono::system_clock::now(), tp);
        }

        bool UserTimeBaseProvider::IsSourceAvailable() const
        {
            return static_cast<bool>(mSource);
        }

        const char *UserTimeBaseProvider::GetProviderName() const noexcept
        {
            return mName.c_str();
        }
    }
}
