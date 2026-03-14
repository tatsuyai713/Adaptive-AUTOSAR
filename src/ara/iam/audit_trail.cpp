/// @file src/ara/iam/audit_trail.cpp
/// @brief Implementation of AuditTrail (SWS_IAM_00301).

#include "./audit_trail.h"

namespace ara
{
    namespace iam
    {
        void AuditTrail::Record(const AuditRecord &entry)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntries.push_back(entry);
        }

        std::vector<AuditRecord> AuditTrail::GetEntries() const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEntries;
        }

        std::size_t AuditTrail::GetEntryCount() const noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mEntries.size();
        }

        void AuditTrail::Clear() noexcept
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntries.clear();
        }
    }
}
