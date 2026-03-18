#include <gtest/gtest.h>
#include "../../../src/ara/exec/manifest_loader.h"

namespace ara
{
    namespace exec
    {
        TEST(ManifestLoaderTest, LoadEmptyStringReturnsWarning)
        {
            ExecutionManifestLoader _loader;
            auto _result = _loader.LoadFromString("");
            EXPECT_FALSE(_result.Entries.empty() && _result.Warnings.empty());
        }

        TEST(ManifestLoaderTest, LoadValidArxmlContent)
        {
            ExecutionManifestLoader _loader;
            std::string _xml =
                "<PROCESS-DESIGN>"
                "<SHORT-NAME>MyApp</SHORT-NAME>"
                "<EXECUTABLE-REF>/opt/bin/my_app</EXECUTABLE-REF>"
                "<FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>"
                "<STATE-REF>Running</STATE-REF>"
                "<STARTUP-PRIORITY>10</STARTUP-PRIORITY>"
                "</PROCESS-DESIGN>";

            auto _result = _loader.LoadFromString(_xml);
            EXPECT_TRUE(_result.Success);
            ASSERT_EQ(_result.Entries.size(), 1U);
            EXPECT_EQ(_result.Entries[0].Descriptor.name, "MyApp");
            EXPECT_EQ(_result.Entries[0].Descriptor.executable,
                       "/opt/bin/my_app");
            EXPECT_EQ(_result.Entries[0].StartupPriority, 10U);
        }

        TEST(ManifestLoaderTest, LoadMultipleProcesses)
        {
            ExecutionManifestLoader _loader;
            std::string _xml =
                "<PROCESS-DESIGN>"
                "<SHORT-NAME>App1</SHORT-NAME>"
                "<EXECUTABLE-REF>/bin/app1</EXECUTABLE-REF>"
                "</PROCESS-DESIGN>"
                "<PROCESS-DESIGN>"
                "<SHORT-NAME>App2</SHORT-NAME>"
                "<EXECUTABLE-REF>/bin/app2</EXECUTABLE-REF>"
                "</PROCESS-DESIGN>";

            auto _result = _loader.LoadFromString(_xml);
            EXPECT_EQ(_result.Entries.size(), 2U);
        }

        TEST(ManifestLoaderTest, ValidateDetectsDuplicateNames)
        {
            ExecutionManifestLoader _loader;
            ManifestProcessEntry _e1, _e2;
            _e1.Descriptor.name = "App1";
            _e2.Descriptor.name = "App1";
            auto _warnings = _loader.ValidateEntries({_e1, _e2});
            EXPECT_FALSE(_warnings.empty());
        }

        TEST(ManifestLoaderTest, ValidateDetectsCircularDeps)
        {
            ExecutionManifestLoader _loader;
            ManifestProcessEntry _e1, _e2;
            _e1.Descriptor.name = "A";
            _e1.Dependencies = {"B"};
            _e2.Descriptor.name = "B";
            _e2.Dependencies = {"A"};
            auto _warnings = _loader.ValidateEntries({_e1, _e2});
            EXPECT_FALSE(_warnings.empty());
        }

        TEST(ManifestLoaderTest, SortByStartupOrder)
        {
            ExecutionManifestLoader _loader;
            ManifestProcessEntry _e1, _e2, _e3;
            _e1.Descriptor.name = "C";
            _e1.StartupPriority = 30;
            _e2.Descriptor.name = "A";
            _e2.StartupPriority = 10;
            _e3.Descriptor.name = "B";
            _e3.StartupPriority = 20;
            _e3.Dependencies = {"A"};

            auto _sorted = _loader.SortByStartupOrder({_e1, _e2, _e3});
            ASSERT_EQ(_sorted.size(), 3U);
            EXPECT_EQ(_sorted[0].Descriptor.name, "A");
        }

        TEST(ManifestLoaderTest, LoadFromNonExistentFile)
        {
            ExecutionManifestLoader _loader;
            auto _result = _loader.LoadFromFile("/nonexistent/path.arxml");
            EXPECT_FALSE(_result.Success);
            EXPECT_FALSE(_result.Warnings.empty());
        }

        TEST(ManifestLoaderTest, DependencyParsing)
        {
            ExecutionManifestLoader _loader;
            std::string _xml =
                "<PROCESS-DESIGN>"
                "<SHORT-NAME>App</SHORT-NAME>"
                "<EXECUTABLE-REF>/bin/app</EXECUTABLE-REF>"
                "<DEPENDS-ON>Dep1, Dep2</DEPENDS-ON>"
                "</PROCESS-DESIGN>";

            auto _result = _loader.LoadFromString(_xml);
            ASSERT_EQ(_result.Entries.size(), 1U);
            EXPECT_EQ(_result.Entries[0].Dependencies.size(), 2U);
        }
    }
}
