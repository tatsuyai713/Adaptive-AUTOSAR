/// @file src/ara/iam/audit_trail.h
/// @brief Audit trail for access control decisions (SWS_IAM_00300-00301).

#ifndef ARA_IAM_AUDIT_TRAIL_H
#define ARA_IAM_AUDIT_TRAIL_H

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "../core/result.h"

namespace ara
{
    namespace iam
    {
        /// @brief Single audit record (SWS_IAM_00300).
        struct AuditRecord
        {
            std::chrono::system_clock::time_point timestamp;
            std::string subjectId;
            std::string resourceId;
            std::string action;
            bool granted{false};
            std::string reason;
        };

        /// @brief Append-only audit trail (SWS_IAM_00301).
        class AuditTrail
        {
        public:
            AuditTrail() = default;
            ~AuditTrail() noexcept = default;

            /// @brief Record an access control decision.
            void Record(const AuditRecord &entry);

            /// @brief Get all recorded entries.
            std::vector<AuditRecord> GetEntries() const;

            /// @brief Get the number of recorded entries.
            std::size_t GetEntryCount() const noexcept;

            /// @brief Clear all entries.
            void Clear() noexcept;

        private:
            std::vector<AuditRecord> mEntries;
            mutable std::mutex mMutex;
        };

    } // namespace iam
} // namespace ara

#endif // ARA_IAM_AUDIT_TRAIL_H
