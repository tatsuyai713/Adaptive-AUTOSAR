#include <iostream>
#include "./argument_configuration.h"

namespace application
{
    namespace helper
    {
        const std::string ArgumentConfiguration::cConfigArgument{"config"};
        const std::string ArgumentConfiguration::cEvConfigArgument{"evconfig"};
        const std::string ArgumentConfiguration::cDmConfigArgument{"dmconfig"};
        const std::string ArgumentConfiguration::cPhmConfigArgument{"phmconfig"};

        ArgumentConfiguration::ArgumentConfiguration(
            int argc,
            char *argv[],
            std::string defaultConfigFile,
            std::string extendedVehicleConfigFile,
            std::string diagnosticManagerConfigFile,
            std::string healthMonitoringConfigFile)
        {
            const int cConfigArgumentIndex{1};
            const int cEvConfigArgumentIndex{2};
            const int cDmConfigArgumentIndex{3};
            const int cPhmConfigArgumentIndex{4};

            if (argc > cPhmConfigArgumentIndex)
            {
                std::string _configFilepath{argv[cConfigArgumentIndex]};
                mArguments[cConfigArgument] = _configFilepath;

                std::string _evConfigFilepath{argv[cEvConfigArgumentIndex]};
                mArguments[cEvConfigArgument] = _evConfigFilepath;

                std::string _dmConfigFilepath{argv[cDmConfigArgumentIndex]};
                mArguments[cDmConfigArgument] = _dmConfigFilepath;

                std::string _phmConfigFilepath{argv[cPhmConfigArgumentIndex]};
                mArguments[cPhmConfigArgument] = _phmConfigFilepath;
            }
            else
            {
                mArguments[cConfigArgument] = defaultConfigFile;
                mArguments[cEvConfigArgument] = extendedVehicleConfigFile;
                mArguments[cDmConfigArgument] = diagnosticManagerConfigFile;
                mArguments[cPhmConfigArgument] = healthMonitoringConfigFile;
            }
        }

        const std::map<std::string, std::string> &ArgumentConfiguration::GetArguments() const noexcept
        {
            return mArguments;
        }
    }
}
