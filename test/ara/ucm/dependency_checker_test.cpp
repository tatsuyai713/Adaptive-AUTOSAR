#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <unordered_map>
#include "../../../src/ara/ucm/dependency_checker.h"

namespace ara
{
    namespace ucm
    {
        TEST(DependencyCheckerTest, AddAndQueryDependency)
        {
            DependencyChecker _checker;

            DependencyConstraint _dep;
            _dep.SourceCluster = "AppCluster";
            _dep.RequiredCluster = "BaseCluster";
            _dep.MinVersion = "1.0.0";

            auto _result = _checker.AddDependency(_dep);
            EXPECT_TRUE(_result.HasValue());

            auto _deps = _checker.GetDependenciesForCluster("AppCluster");
            EXPECT_EQ(_deps.size(), 1U);
            EXPECT_EQ(_deps[0].RequiredCluster, "BaseCluster");
        }

        TEST(DependencyCheckerTest, AddInvalidDependencyFails)
        {
            DependencyChecker _checker;

            DependencyConstraint _dep;
            _dep.SourceCluster = "";
            _dep.RequiredCluster = "BaseCluster";

            auto _result = _checker.AddDependency(_dep);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(DependencyCheckerTest, CheckDependenciesSatisfied)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "1.0.0", ""});

            std::unordered_map<std::string, std::string> versions;
            versions["Base"] = "2.0.0";

            auto _result = _checker.CheckDependencies("App", versions);
            EXPECT_TRUE(_result.HasValue());
        }

        TEST(DependencyCheckerTest, CheckDependenciesMinVersionFail)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "3.0.0", ""});

            std::unordered_map<std::string, std::string> versions;
            versions["Base"] = "2.0.0";

            auto _result = _checker.CheckDependencies("App", versions);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(DependencyCheckerTest, CheckDependenciesMaxVersionFail)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "1.0.0", "2.0.0"});

            std::unordered_map<std::string, std::string> versions;
            versions["Base"] = "3.0.0";

            auto _result = _checker.CheckDependencies("App", versions);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(DependencyCheckerTest, CheckDependenciesMissingCluster)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "1.0.0", ""});

            std::unordered_map<std::string, std::string> versions;
            // "Base" not present

            auto _result = _checker.CheckDependencies("App", versions);
            EXPECT_FALSE(_result.HasValue());
        }

        TEST(DependencyCheckerTest, CheckAllDependencies)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "1.0.0", ""});
            _checker.AddDependency({"App", "Crypto", "2.0.0", ""});

            std::unordered_map<std::string, std::string> versions;
            versions["Base"] = "1.0.0";
            versions["Crypto"] = "1.5.0"; // too old

            auto results = _checker.CheckAllDependencies(versions);
            EXPECT_EQ(results.size(), 2U);

            bool baseSatisfied = false;
            bool cryptoSatisfied = false;
            for (const auto &r : results)
            {
                if (r.Constraint.RequiredCluster == "Base")
                {
                    baseSatisfied = r.Satisfied;
                }
                if (r.Constraint.RequiredCluster == "Crypto")
                {
                    cryptoSatisfied = r.Satisfied;
                }
            }
            EXPECT_TRUE(baseSatisfied);
            EXPECT_FALSE(cryptoSatisfied);
        }

        TEST(DependencyCheckerTest, RemoveDependencies)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"App", "Base", "1.0.0", ""});
            _checker.AddDependency({"App", "Crypto", "1.0.0", ""});
            _checker.AddDependency({"Other", "Base", "1.0.0", ""});

            auto removed = _checker.RemoveDependenciesForCluster("App");
            EXPECT_TRUE(removed.HasValue());
            EXPECT_EQ(removed.Value(), 2U);

            EXPECT_EQ(_checker.GetAllDependencies().size(), 1U);
        }

        TEST(DependencyCheckerTest, SaveAndLoadRoundTrip)
        {
            const std::string _path{"/tmp/autosar_test_dep_checker.csv"};

            {
                DependencyChecker _checker;
                _checker.AddDependency({"App", "Base", "1.0.0", "5.0.0"});
                _checker.AddDependency({"Svc", "Crypto", "2.0", ""});
                _checker.SaveToFile(_path);
            }

            {
                DependencyChecker _checker;
                auto _result = _checker.LoadFromFile(_path);
                EXPECT_TRUE(_result.HasValue());

                auto _all = _checker.GetAllDependencies();
                EXPECT_EQ(_all.size(), 2U);
                EXPECT_EQ(_all[0].SourceCluster, "App");
                EXPECT_EQ(_all[0].MaxVersion, "5.0.0");
                EXPECT_EQ(_all[1].SourceCluster, "Svc");
            }

            std::remove(_path.c_str());
        }

        TEST(DependencyCheckerTest, Clear)
        {
            DependencyChecker _checker;
            _checker.AddDependency({"A", "B", "1.0", ""});
            _checker.Clear();
            EXPECT_EQ(_checker.GetAllDependencies().size(), 0U);
        }
    }
}
