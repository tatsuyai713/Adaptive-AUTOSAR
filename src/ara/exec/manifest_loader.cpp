/// @file src/ara/exec/manifest_loader.cpp
/// @brief Implementation of ExecutionManifestLoader.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./manifest_loader.h"
#include "./exec_error_domain.h"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace ara
{
    namespace exec
    {
        ManifestLoadResult ExecutionManifestLoader::LoadFromFile(
            const std::string &filePath) const
        {
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                ManifestLoadResult result;
                result.Warnings.push_back(
                    "Failed to open manifest file: " + filePath);
                return result;
            }

            std::ostringstream oss;
            oss << file.rdbuf();
            return LoadFromString(oss.str());
        }

        ManifestLoadResult ExecutionManifestLoader::LoadFromString(
            const std::string &xmlContent) const
        {
            ManifestLoadResult result;

            if (xmlContent.empty())
            {
                result.Warnings.push_back("Empty manifest content");
                return result;
            }

            // Lightweight tag-pair parser for <PROCESS-DESIGN> elements.
            const std::string cOpenTag = "<PROCESS-DESIGN";
            const std::string cCloseTag = "</PROCESS-DESIGN>";

            std::string::size_type pos = 0;
            while ((pos = xmlContent.find(cOpenTag, pos)) != std::string::npos)
            {
                auto closePos = xmlContent.find(cCloseTag, pos);
                if (closePos == std::string::npos)
                {
                    result.Warnings.push_back(
                        "Unterminated <PROCESS-DESIGN> at offset " +
                        std::to_string(pos));
                    break;
                }

                std::string block = xmlContent.substr(
                    pos, closePos + cCloseTag.size() - pos);

                // Extract attributes from the block.
                std::map<std::string, std::string> attrs;
                auto extractTag = [&](const std::string &tag) -> std::string {
                    std::string open = "<" + tag + ">";
                    std::string close = "</" + tag + ">";
                    auto s = block.find(open);
                    if (s == std::string::npos) return "";
                    s += open.size();
                    auto e = block.find(close, s);
                    if (e == std::string::npos) return "";
                    return block.substr(s, e - s);
                };

                attrs["SHORT-NAME"] = extractTag("SHORT-NAME");
                attrs["EXECUTABLE-REF"] = extractTag("EXECUTABLE-REF");
                attrs["FUNCTION-GROUP-REF"] = extractTag("FUNCTION-GROUP-REF");
                attrs["STATE-REF"] = extractTag("STATE-REF");
                attrs["STARTUP-PRIORITY"] = extractTag("STARTUP-PRIORITY");
                attrs["STARTUP-GRACE-MS"] = extractTag("STARTUP-GRACE-MS");
                attrs["TERMINATION-TIMEOUT-MS"] =
                    extractTag("TERMINATION-TIMEOUT-MS");
                attrs["CPU-QUOTA-PERCENT"] = extractTag("CPU-QUOTA-PERCENT");
                attrs["MEMORY-LIMIT-BYTES"] = extractTag("MEMORY-LIMIT-BYTES");
                attrs["DEPENDS-ON"] = extractTag("DEPENDS-ON");

                if (attrs["SHORT-NAME"].empty())
                {
                    result.Warnings.push_back(
                        "Missing SHORT-NAME in PROCESS-DESIGN block");
                }
                else
                {
                    result.Entries.push_back(
                        ParseProcessNode(attrs["SHORT-NAME"], attrs));
                }

                pos = closePos + cCloseTag.size();
            }

            result.Success = !result.Entries.empty() || result.Warnings.empty();
            return result;
        }

        std::vector<std::string> ExecutionManifestLoader::ValidateEntries(
            const std::vector<ManifestProcessEntry> &entries) const
        {
            std::vector<std::string> warnings;

            std::set<std::string> names;
            for (const auto &e : entries)
            {
                if (e.Descriptor.name.empty())
                {
                    warnings.push_back("Entry with empty process name");
                    continue;
                }
                if (!names.insert(e.Descriptor.name).second)
                {
                    warnings.push_back(
                        "Duplicate process name: " + e.Descriptor.name);
                }
            }

            // Check for circular dependencies (simple DFS).
            std::map<std::string, std::vector<std::string>> depGraph;
            for (const auto &e : entries)
            {
                depGraph[e.Descriptor.name] = e.Dependencies;
            }

            for (const auto &e : entries)
            {
                std::set<std::string> visited;
                std::vector<std::string> stack{e.Descriptor.name};
                while (!stack.empty())
                {
                    auto cur = stack.back();
                    stack.pop_back();
                    if (!visited.insert(cur).second)
                    {
                        warnings.push_back(
                            "Circular dependency detected involving: " + cur);
                        break;
                    }
                    auto it = depGraph.find(cur);
                    if (it != depGraph.end())
                    {
                        for (const auto &d : it->second)
                        {
                            stack.push_back(d);
                        }
                    }
                }
            }

            // Check for dependencies on unknown processes.
            for (const auto &e : entries)
            {
                for (const auto &dep : e.Dependencies)
                {
                    if (names.find(dep) == names.end())
                    {
                        warnings.push_back(
                            "Process '" + e.Descriptor.name +
                            "' depends on unknown process: " + dep);
                    }
                }
            }

            return warnings;
        }

        std::vector<ManifestProcessEntry>
        ExecutionManifestLoader::SortByStartupOrder(
            std::vector<ManifestProcessEntry> entries) const
        {
            // Topological sort based on dependencies, then priority.
            std::map<std::string, size_t> nameIndex;
            for (size_t i = 0; i < entries.size(); ++i)
            {
                nameIndex[entries[i].Descriptor.name] = i;
            }

            std::vector<size_t> order;
            std::set<size_t> visited;
            std::set<size_t> inStack;

            std::function<void(size_t)> visit = [&](size_t idx) {
                if (visited.count(idx)) return;
                if (inStack.count(idx)) return; // cycle guard
                inStack.insert(idx);

                for (const auto &dep : entries[idx].Dependencies)
                {
                    auto it = nameIndex.find(dep);
                    if (it != nameIndex.end())
                    {
                        visit(it->second);
                    }
                }

                inStack.erase(idx);
                visited.insert(idx);
                order.push_back(idx);
            };

            // Sort by priority first, then apply topo-sort.
            std::vector<size_t> indices(entries.size());
            for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
            std::sort(indices.begin(), indices.end(),
                      [&](size_t a, size_t b) {
                          return entries[a].StartupPriority <
                                 entries[b].StartupPriority;
                      });

            for (auto idx : indices) visit(idx);

            std::vector<ManifestProcessEntry> sorted;
            sorted.reserve(order.size());
            for (auto idx : order) sorted.push_back(std::move(entries[idx]));
            return sorted;
        }

        ManifestProcessEntry ExecutionManifestLoader::ParseProcessNode(
            const std::string &nodeName,
            const std::map<std::string, std::string> &attributes) const
        {
            ManifestProcessEntry entry;
            entry.Descriptor.name = nodeName;

            auto getAttr = [&](const std::string &key) -> std::string {
                auto it = attributes.find(key);
                return (it != attributes.end()) ? it->second : "";
            };

            entry.Descriptor.executable = getAttr("EXECUTABLE-REF");
            entry.Descriptor.functionGroup = getAttr("FUNCTION-GROUP-REF");
            entry.Descriptor.activeState = getAttr("STATE-REF");

            auto priority = getAttr("STARTUP-PRIORITY");
            if (!priority.empty())
            {
                entry.StartupPriority =
                    static_cast<uint32_t>(std::stoul(priority));
            }

            auto graceMs = getAttr("STARTUP-GRACE-MS");
            if (!graceMs.empty())
            {
                entry.Descriptor.startupGrace =
                    std::chrono::milliseconds(std::stoul(graceMs));
            }

            auto termMs = getAttr("TERMINATION-TIMEOUT-MS");
            if (!termMs.empty())
            {
                entry.Descriptor.terminationTimeout =
                    std::chrono::milliseconds(std::stoul(termMs));
            }

            auto cpuQ = getAttr("CPU-QUOTA-PERCENT");
            if (!cpuQ.empty())
            {
                entry.CpuQuotaPercent =
                    static_cast<uint32_t>(std::stoul(cpuQ));
            }

            auto memL = getAttr("MEMORY-LIMIT-BYTES");
            if (!memL.empty())
            {
                entry.MemoryLimitBytes = std::stoull(memL);
            }

            auto deps = getAttr("DEPENDS-ON");
            if (!deps.empty())
            {
                std::istringstream ss(deps);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    // Trim whitespace.
                    auto start = token.find_first_not_of(" \t");
                    auto end = token.find_last_not_of(" \t");
                    if (start != std::string::npos)
                    {
                        entry.Dependencies.push_back(
                            token.substr(start, end - start + 1));
                    }
                }
            }

            return entry;
        }
    }
}
