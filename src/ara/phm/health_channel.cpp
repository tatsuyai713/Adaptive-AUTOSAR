#include "./health_channel.h"

namespace ara
{
    namespace phm
    {
        HealthChannel::HealthChannel(
            core::InstanceSpecifier instance) : mInstance{std::move(instance)},
                                                mLastReportedStatus{HealthStatus::kOk},
                                                mOffered{false}
        {
        }

        HealthChannel::HealthChannel(HealthChannel &&other) noexcept
            : mInstance{std::move(other.mInstance)},
              mLastReportedStatus{other.mLastReportedStatus},
              mOffered{other.mOffered}
        {
            other.mOffered = false;
        }

        core::Result<void> HealthChannel::ReportHealthStatus(HealthStatus status)
        {
            mLastReportedStatus = status;
            return core::Result<void>::FromValue();
        }

        HealthStatus HealthChannel::GetLastReportedStatus() const noexcept
        {
            return mLastReportedStatus;
        }
    }
}
