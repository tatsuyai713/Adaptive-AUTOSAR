/// @file src/ara/ucm/dependency_checker.cpp
/// @brief Implementation for software package dependency checker.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./dependency_checker.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace ara
{
    namespace ucm
    {
        // -----------------------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------------------

        int DependencyChecker::CompareVersions(
            const std::string &a,
            const std::string &b)
        {
            std::istringstream sa(a);
            std::istringstream sb(b);
            std::string partA;
            std::string partB;

            while (true)
            {
                const bool hasA = static_cast<bool>(std::getline(sa, partA, '.'));
                const bool hasB = static_cast<bool>(std::getline(sb, partB, '.'));

                if (!hasA && !hasB)
                {
                    return 0; // equal
                }

                const int numA = hasA ? std::atoi(partA.c_str()) : 0;
                const int numB = hasB ? std::atoi(partB.c_str()) : 0;

                if (numA < numB)
                {
                    return -1;
                }
                if (numA > numB)
                {
                    return 1;
                }
            }
        }

        // -----------------------------------------------------------------------
        // Public API
        // -----------------------------------------------------------------------

        core::Result<void> DependencyChecker::AddDependency(
            const DependencyConstraint &constraint)
        {
            if (constraint.SourceCluster.empty() ||
                constraint.RequiredCluster.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> lock{mMutex};
            mConstraints.push_back(constraint);
            return core::Result<void>::FromValue();
        }

        core::Result<std::size_t> DependencyChecker::RemoveDependenciesForCluster(
            const std::string &sourceCluster)
        {
            std::lock_guard<std::mutex> lock{mMutex};
            const auto origSize = mConstraints.size();
            mConstraints.erase(
                std::remove_if(
                    mConstraints.begin(), mConstraints.end(),
                    [&](const DependencyConstraint &c)
                    {
                        return c.SourceCluster == sourceCluster;
                    }),
                mConstraints.end());
            return core::Result<std::size_t>::FromValue(
                origSize - mConstraints.size());
        }

        std::vector<DependencyConstraint>
        DependencyChecker::GetDependenciesForCluster(
            const std::string &sourceCluster) const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            std::vector<DependencyConstraint> result;
            for (const auto &c : mConstraints)
            {
                if (c.SourceCluster == sourceCluster)
                {
                    result.push_back(c);
                }
            }
            return result;
        }

        std::vector<DependencyConstraint>
        DependencyChecker::GetAllDependencies() const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            return mConstraints;
        }

        core::Result<void> DependencyChecker::CheckDependencies(
            const std::string &sourceCluster,
            const std::unordered_map<std::string, std::string> &clusterVersions) const
        {
            std::lock_guard<std::mutex> lock{mMutex};

            for (const auto &c : mConstraints)
            {
                if (c.SourceCluster != sourceCluster)
                {
                    continue;
                }

                auto it = clusterVersions.find(c.RequiredCluster);
                if (it == clusterVersions.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kClusterNotFound));
                }

                const auto &presentVersion = it->second;

                // Check minimum version
                if (!c.MinVersion.empty())
                {
                    if (CompareVersions(presentVersion, c.MinVersion) < 0)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(UcmErrc::kVerificationFailed));
                    }
                }

                // Check maximum version
                if (!c.MaxVersion.empty())
                {
                    if (CompareVersions(presentVersion, c.MaxVersion) > 0)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(UcmErrc::kVerificationFailed));
                    }
                }
            }

            return core::Result<void>::FromValue();
        }

        std::vector<DependencyCheckResult>
        DependencyChecker::CheckAllDependencies(
            const std::unordered_map<std::string, std::string> &clusterVersions) const
        {
            std::lock_guard<std::mutex> lock{mMutex};
            std::vector<DependencyCheckResult> results;

            for (const auto &c : mConstraints)
            {
                DependencyCheckResult r;
                r.Constraint = c;

                auto it = clusterVersions.find(c.RequiredCluster);
                if (it == clusterVersions.end())
                {
                    r.Satisfied = false;
                    r.Reason = "Required cluster not found: " +
                               c.RequiredCluster;
                    results.push_back(std::move(r));
                    continue;
                }

                r.PresentVersion = it->second;
                bool ok = true;

                if (!c.MinVersion.empty() &&
                    CompareVersions(r.PresentVersion, c.MinVersion) < 0)
                {
                    ok = false;
                    r.Reason = "Version " + r.PresentVersion +
                               " < required minimum " + c.MinVersion;
                }

                if (ok && !c.MaxVersion.empty() &&
                    CompareVersions(r.PresentVersion, c.MaxVersion) > 0)
                {
                    ok = false;
                    r.Reason = "Version " + r.PresentVersion +
                               " > allowed maximum " + c.MaxVersion;
                }

                if (ok)
                {
                    r.Satisfied = true;
                }

                results.push_back(std::move(r));
            }

            return results;
        }

        core::Result<void> DependencyChecker::SaveToFile(
            const std::string &filePath) const
        {
            std::lock_guard<std::mutex> lock{mMutex};

            std::ofstream ofs(filePath);
            if (!ofs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kHistoryError));
            }

            for (const auto &c : mConstraints)
            {
                ofs << c.SourceCluster << "|"
                    << c.RequiredCluster << "|"
                    << c.MinVersion << "|"
                    << c.MaxVersion << "\n";
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> DependencyChecker::LoadFromFile(
            const std::string &filePath)
        {
            std::lock_guard<std::mutex> lock{mMutex};

            std::ifstream ifs(filePath);
            if (!ifs.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kHistoryError));
            }

            mConstraints.clear();
            std::string line;
            while (std::getline(ifs, line))
            {
                if (line.empty())
                {
                    continue;
                }

                std::istringstream ss(line);
                DependencyConstraint c;
                std::getline(ss, c.SourceCluster, '|');
                std::getline(ss, c.RequiredCluster, '|');
                std::getline(ss, c.MinVersion, '|');
                std::getline(ss, c.MaxVersion, '|');

                if (!c.SourceCluster.empty() &&
                    !c.RequiredCluster.empty())
                {
                    mConstraints.push_back(std::move(c));
                }
            }

            return core::Result<void>::FromValue();
        }

        void DependencyChecker::Clear()
        {
            std::lock_guard<std::mutex> lock{mMutex};
            mConstraints.clear();
        }

    } // namespace ucm
} // namespace ara
