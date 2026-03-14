/// @file src/ara/iam/capability_manager.h
/// @brief Capability-based access control manager (SWS_IAM_00200).

#ifndef ARA_IAM_CAPABILITY_MANAGER_H
#define ARA_IAM_CAPABILITY_MANAGER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include "../core/result.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Capability-based access control manager (SWS_IAM_00200).
        /// @details Manages per-subject capability sets. A capability is an
        ///          opaque string token granting access to a resource or action.
        class CapabilityManager
        {
        public:
            CapabilityManager() = default;
            ~CapabilityManager() noexcept = default;

            /// @brief Grant a capability to a subject.
            core::Result<void> Grant(
                const std::string &subjectId,
                const std::string &capability);

            /// @brief Revoke a capability from a subject.
            core::Result<void> Revoke(
                const std::string &subjectId,
                const std::string &capability);

            /// @brief Check if a subject holds a capability.
            bool HasCapability(
                const std::string &subjectId,
                const std::string &capability) const;

            /// @brief Get all capabilities held by a subject.
            std::set<std::string> GetCapabilities(
                const std::string &subjectId) const;

            /// @brief Remove all capabilities for a subject.
            void ClearCapabilities(const std::string &subjectId);

        private:
            std::map<std::string, std::set<std::string>> mCapabilities;
            mutable std::mutex mMutex;
        };

    } // namespace iam
} // namespace ara

#endif // ARA_IAM_CAPABILITY_MANAGER_H
