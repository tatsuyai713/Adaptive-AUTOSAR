/// @file src/ara/iam/role_manager.cpp
/// @brief Implementation for role-based access control.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./role_manager.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ara
{
    namespace iam
    {
        // -------------------------------------------------------------------------
        // Private helpers
        // -------------------------------------------------------------------------

        void RoleManager::collectInheritedRoles(
            const std::string &roleName,
            std::set<std::string> &visited) const
        {
            if (visited.count(roleName) != 0U)
            {
                return; // already visited — stop recursion
            }
            visited.insert(roleName);

            const auto cIt{mRoles.find(roleName)};
            if (cIt == mRoles.end())
            {
                return; // unknown role
            }

            const std::string &_parent{cIt->second};
            if (!_parent.empty())
            {
                collectInheritedRoles(_parent, visited);
            }
        }

        bool RoleManager::wouldCreateCycle(
            const std::string &roleName,
            const std::string &parentRole) const
        {
            // Walk up the ancestor chain of parentRole; if we reach roleName, cycle.
            std::string _cur{parentRole};
            while (!_cur.empty())
            {
                if (_cur == roleName)
                {
                    return true;
                }
                const auto cIt{mRoles.find(_cur)};
                if (cIt == mRoles.end())
                {
                    break;
                }
                _cur = cIt->second;
            }
            return false;
        }

        // -------------------------------------------------------------------------
        // Public API
        // -------------------------------------------------------------------------

        core::Result<void> RoleManager::AddRole(
            const std::string &roleName,
            const std::string &parentRole)
        {
            if (roleName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            if (mRoles.count(roleName) != 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            if (!parentRole.empty())
            {
                if (mRoles.count(parentRole) == 0U)
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kInvalidArgument));
                }

                if (wouldCreateCycle(roleName, parentRole))
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kInvalidArgument));
                }
            }

            mRoles.emplace(roleName, parentRole);
            return core::Result<void>::FromValue();
        }

        core::Result<void> RoleManager::RemoveRole(const std::string &roleName)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            if (mRoles.count(roleName) == 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            mRoles.erase(roleName);

            // Remove subject assignments for this role
            for (auto &_pair : mAssignments)
            {
                _pair.second.erase(roleName);
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> RoleManager::AssignRole(
            const std::string &subject,
            const std::string &roleName)
        {
            if (subject.empty() || roleName.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            if (mRoles.count(roleName) == 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            auto &_roleSet{mAssignments[subject]};
            if (_roleSet.count(roleName) != 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            _roleSet.insert(roleName);
            return core::Result<void>::FromValue();
        }

        core::Result<void> RoleManager::RevokeRole(
            const std::string &subject,
            const std::string &roleName)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            const auto cSubjIt{mAssignments.find(subject)};
            if (cSubjIt == mAssignments.end() ||
                cSubjIt->second.count(roleName) == 0U)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kInvalidArgument));
            }

            cSubjIt->second.erase(roleName);
            return core::Result<void>::FromValue();
        }

        bool RoleManager::HasRole(
            const std::string &subject,
            const std::string &roleName) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            const auto cSubjIt{mAssignments.find(subject)};
            if (cSubjIt == mAssignments.end())
            {
                return false;
            }

            for (const auto &cDirectRole : cSubjIt->second)
            {
                std::set<std::string> _visited;
                collectInheritedRoles(cDirectRole, _visited);
                if (_visited.count(roleName) != 0U)
                {
                    return true;
                }
            }

            return false;
        }

        std::vector<std::string> RoleManager::GetRolesForSubject(
            const std::string &subject) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::set<std::string> _allRoles;

            const auto cSubjIt{mAssignments.find(subject)};
            if (cSubjIt != mAssignments.end())
            {
                for (const auto &cDirectRole : cSubjIt->second)
                {
                    collectInheritedRoles(cDirectRole, _allRoles);
                }
            }

            return std::vector<std::string>(_allRoles.begin(), _allRoles.end());
        }

        core::Result<bool> RoleManager::IsAllowedViaRole(
            const std::string &subject,
            const std::string &resource,
            const std::string &action,
            const AccessControl &accessControl) const
        {
            const auto cRoles{GetRolesForSubject(subject)};

            for (const auto &cRole : cRoles)
            {
                const auto cDecision{accessControl.IsAllowed(cRole, resource, action)};
                if (cDecision.HasValue() && cDecision.Value())
                {
                    return core::Result<bool>::FromValue(true);
                }
            }

            return core::Result<bool>::FromValue(false);
        }

        core::Result<void> RoleManager::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::ofstream _file{filePath};
            if (!_file)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            // Format: "ROLE <name> <parent|->
            for (const auto &cEntry : mRoles)
            {
                _file << "ROLE " << cEntry.first << " "
                      << (cEntry.second.empty() ? "-" : cEntry.second) << "\n";
            }

            // Format: "ASSIGN <subject> <role>"
            for (const auto &cEntry : mAssignments)
            {
                for (const auto &cRole : cEntry.second)
                {
                    _file << "ASSIGN " << cEntry.first << " " << cRole << "\n";
                }
            }

            if (!_file)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> RoleManager::LoadFromFile(
            const std::string &filePath)
        {
            std::ifstream _file{filePath};
            if (!_file)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(IamErrc::kPolicyStoreError));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            std::string _line;
            std::size_t _lineNum{0U};
            while (std::getline(_file, _line))
            {
                ++_lineNum;
                if (_line.empty() || _line[0] == '#')
                {
                    continue;
                }

                std::istringstream _ss{_line};
                std::string _token;
                _ss >> _token;

                if (_token == "ROLE")
                {
                    std::string _name;
                    std::string _parent;
                    _ss >> _name >> _parent;
                    if (_name.empty() || _parent.empty())
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(IamErrc::kPolicyFileParseError));
                    }
                    if (_parent == "-")
                    {
                        _parent.clear();
                    }
                    mRoles[_name] = _parent;
                }
                else if (_token == "ASSIGN")
                {
                    std::string _subject;
                    std::string _role;
                    _ss >> _subject >> _role;
                    if (_subject.empty() || _role.empty())
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(IamErrc::kPolicyFileParseError));
                    }
                    mAssignments[_subject].insert(_role);
                }
                else
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(IamErrc::kPolicyFileParseError));
                }
            }

            return core::Result<void>::FromValue();
        }
    }
}
