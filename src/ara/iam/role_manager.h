/// @file src/ara/iam/role_manager.h
/// @brief Role-Based Access Control (RBAC) for ara::iam.
/// @details Provides role definitions, subject-role assignments, and role
///          hierarchy support.  Subjects (processes/apps) are assigned one or
///          more named roles.  Each role may optionally inherit permissions from
///          a parent role.  The role graph must be acyclic (a cycle results in
///          an error on AddRole).
///
///          Integration with AccessControl:
///          Use RoleManager::IsAllowedViaRole() to check whether a subject
///          has permission through any of its assigned (and inherited) roles.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ROLE_MANAGER_H
#define ROLE_MANAGER_H

#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/result.h"
#include "./access_control.h"
#include "./iam_error_domain.h"

namespace ara
{
    namespace iam
    {
        /// @brief Role-Based Access Control manager.
        ///
        /// Thread-safe. Supports:
        /// - Role definition with optional single-parent inheritance.
        /// - Subject-to-role assignment (many-to-many).
        /// - Transitive permission lookup via inherited roles.
        /// - File persistence (simple text format).
        class RoleManager
        {
        public:
            RoleManager() = default;

            RoleManager(const RoleManager &) = delete;
            RoleManager &operator=(const RoleManager &) = delete;

            /// @brief Define a new role.
            /// @param roleName Unique role name (must not be empty).
            /// @param parentRole Name of the parent role to inherit from
            ///                   (empty = no parent).
            /// @returns Ok, or IamErrc::kInvalidArgument if the name is empty,
            ///          the role already exists, the parent is unknown, or adding
            ///          the parent would create a cycle.
            core::Result<void> AddRole(
                const std::string &roleName,
                const std::string &parentRole = "");

            /// @brief Remove a role definition and all subject assignments for it.
            /// @param roleName Role to remove.
            /// @returns Ok, or IamErrc::kInvalidArgument if the role does not exist.
            core::Result<void> RemoveRole(const std::string &roleName);

            /// @brief Assign a role to a subject (process/app identity).
            /// @param subject Subject identifier.
            /// @param roleName Role name (must already be defined via AddRole).
            /// @returns Ok, or IamErrc::kInvalidArgument if role is unknown or
            ///          the assignment already exists.
            core::Result<void> AssignRole(
                const std::string &subject,
                const std::string &roleName);

            /// @brief Revoke a role assignment from a subject.
            /// @param subject Subject identifier.
            /// @param roleName Role name.
            /// @returns Ok, or IamErrc::kInvalidArgument if the assignment
            ///          does not exist.
            core::Result<void> RevokeRole(
                const std::string &subject,
                const std::string &roleName);

            /// @brief Check whether a subject has a specific role (direct or inherited).
            /// @param subject Subject identifier.
            /// @param roleName Role name to check.
            /// @returns true if the subject has the role (including via inheritance).
            bool HasRole(
                const std::string &subject,
                const std::string &roleName) const;

            /// @brief Get all roles held by a subject (direct + inherited, de-duplicated).
            /// @param subject Subject identifier.
            /// @returns Sorted vector of role names (empty if subject has no assignments).
            std::vector<std::string> GetRolesForSubject(
                const std::string &subject) const;

            /// @brief Check permission using role-based lookup.
            /// @details Iterates over all roles (direct + inherited) of the subject
            ///          and returns true if any of them is allowed by the AccessControl
            ///          policy.
            /// @param subject Subject identifier.
            /// @param resource Resource/service name.
            /// @param action Operation name.
            /// @param accessControl Policy store to query.
            /// @returns true if access is allowed via any role, false if denied,
            ///          or IAM domain error.
            core::Result<bool> IsAllowedViaRole(
                const std::string &subject,
                const std::string &resource,
                const std::string &action,
                const AccessControl &accessControl) const;

            /// @brief Save role definitions and assignments to a file.
            /// @param filePath Output file path.
            core::Result<void> SaveToFile(const std::string &filePath) const;

            /// @brief Load role definitions and assignments from a file.
            /// @details Appends to existing state.  Re-defines existing roles if the
            ///          file contains duplicate names.
            /// @param filePath Input file path.
            core::Result<void> LoadFromFile(const std::string &filePath);

        private:
            mutable std::mutex mMutex;

            /// @brief role name → parent role name (empty = root role)
            std::unordered_map<std::string, std::string> mRoles;

            /// @brief subject → set of directly assigned role names
            std::unordered_map<std::string, std::set<std::string>> mAssignments;

            /// @brief Collect all roles reachable from roleName via parent chain.
            void collectInheritedRoles(
                const std::string &roleName,
                std::set<std::string> &visited) const;

            /// @brief Return true if adding parentRole as parent of roleName would
            ///        create a cycle in the role graph.
            bool wouldCreateCycle(
                const std::string &roleName,
                const std::string &parentRole) const;
        };
    }
}

#endif
