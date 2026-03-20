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

        namespace
        {
            /// @brief Parse the next dot-delimited integer from a version
            ///        string, advancing pos past the dot. Returns 0 when
            ///        pos is past the end.
            int nextVersionPart(const std::string &s, std::size_t &pos)
            {
                if (pos >= s.size())
                {
                    return 0;
                }
                int value = 0;
                while (pos < s.size() && s[pos] != '.')
                {
                    const char ch = s[pos];
                    if (ch >= '0' && ch <= '9')
                    {
                        value = value * 10 + (ch - '0');
                    }
                    ++pos;
                }
                if (pos < s.size())
                {
                    ++pos; // skip '.'
                }
                return value;
            }
        } // namespace

        int DependencyChecker::CompareVersions(
            const std::string &a,
            const std::string &b)
        {
            std::size_t posA = 0U;
            std::size_t posB = 0U;

            while (posA < a.size() || posB < b.size())
            {
                const int numA = nextVersionPart(a, posA);
                const int numB = nextVersionPart(b, posB);

                if (numA < numB)
                {
                    return -1;
                }
                if (numA > numB)
                {
                    return 1;
                }
            }
            return 0;
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
            // Copy relevant constraints under lock, then check without lock.
            std::vector<DependencyConstraint> relevant;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                for (const auto &c : mConstraints)
                {
                    if (c.SourceCluster == sourceCluster)
                    {
                        relevant.push_back(c);
                    }
                }
            }

            for (const auto &c : relevant)
            {
                auto it = clusterVersions.find(c.RequiredCluster);
                if (it == clusterVersions.end())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kClusterNotFound));
                }

                const auto &presentVersion = it->second;

                if (!c.MinVersion.empty())
                {
                    if (CompareVersions(presentVersion, c.MinVersion) < 0)
                    {
                        return core::Result<void>::FromError(
                            MakeErrorCode(UcmErrc::kVerificationFailed));
                    }
                }

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
            // Copy constraints under lock, then check without lock.
            std::vector<DependencyConstraint> constraints;
            {
                std::lock_guard<std::mutex> lock{mMutex};
                constraints = mConstraints;
            }
            std::vector<DependencyCheckResult> results;

            for (const auto &c : constraints)
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
