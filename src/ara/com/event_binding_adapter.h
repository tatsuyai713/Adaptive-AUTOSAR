/// @file src/ara/com/event_binding_adapter.h
/// @brief Backend-agnostic ara::com pub/sub adapter for CycloneDDS/vsomeip/iceoryx.
/// @details Mapping resolution and CDR conversion live in AUTOSAR runtime side
///          so applications do not depend on non-standard helper APIs directly.

#ifndef ARA_COM_EVENT_BINDING_ADAPTER_H
#define ARA_COM_EVENT_BINDING_ADAPTER_H

#include <ara/com/dds/dds_pubsub.h>
#include <ara/com/event.h>
#include <ara/com/internal/binding_factory.h>
#include <ara/com/sample_ptr.h>
#include <ara/com/service_handle_type.h>
#include <ara/com/service_proxy_base.h>
#include <ara/com/service_skeleton_base.h>
#include <ara/com/zerocopy/zero_copy.h>
#include <ara/core/instance_specifier.h>
#include <ara/core/result.h>

#include <yaml-cpp/yaml.h>
#include <org/eclipse/cyclonedds/core/cdr/basic_cdr_ser.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ara
{
    namespace com
    {
        enum class EventTransportBinding
        {
            kDds,
            kSomeip,
            kIceoryx
        };

        inline std::string NormalizeTransportName(std::string value)
        {
            std::transform(
                value.begin(), value.end(), value.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });
            return value;
        }

        inline bool ParseBoolEnv(const char *env_name)
        {
            const char *value = std::getenv(env_name);
            if (value == nullptr || *value == 0)
            {
                return false;
            }

            std::string normalized = NormalizeTransportName(std::string(value));
            return normalized == "1" || normalized == "true" || normalized == "yes" ||
                   normalized == "on";
        }

        inline bool ParseTransportBindingToken(
            const std::string &value,
            EventTransportBinding &binding,
            bool *isAuto = nullptr)
        {
            if (isAuto != nullptr)
            {
                *isAuto = false;
            }

            const std::string normalized = NormalizeTransportName(value);
            if (normalized.empty())
            {
                return false;
            }

            if (normalized == "someip" || normalized == "vsomeip")
            {
                binding = EventTransportBinding::kSomeip;
                return true;
            }
            if (normalized == "dds" || normalized == "cyclonedds" ||
                normalized == "cyclone-dds")
            {
                binding = EventTransportBinding::kDds;
                return true;
            }
            if (normalized == "iceoryx" || normalized == "zerocopy" ||
                normalized == "zero-copy" || normalized == "iox")
            {
                binding = EventTransportBinding::kIceoryx;
                return true;
            }
            if (normalized == "auto")
            {
                if (isAuto != nullptr)
                {
                    *isAuto = true;
                }
                return true;
            }

            return false;
        }

        inline EventTransportBinding ResolveAutoEventTransportBinding()
        {
            if (ParseBoolEnv("ARA_COM_PREFER_SOMEIP"))
            {
                return EventTransportBinding::kSomeip;
            }
            if (ParseBoolEnv("ARA_COM_PREFER_ICEORYX") ||
                ParseBoolEnv("ARA_COM_PREFER_ZEROCOPY"))
            {
                return EventTransportBinding::kIceoryx;
            }
            return EventTransportBinding::kDds;
        }

        inline std::string ResolveManifestEventBindingToken()
        {
            const char *manifestPath = std::getenv("ARA_COM_BINDING_MANIFEST");
            if (manifestPath == nullptr || *manifestPath == 0)
            {
                return std::string{};
            }

            try
            {
                YAML::Node root{YAML::LoadFile(manifestPath)};
                YAML::Node autosar = root["autosar"];
                if (autosar)
                {
                    YAML::Node packages = autosar["packages"];
                    if (packages && packages.IsSequence())
                    {
                        for (std::size_t i = 0U; i < packages.size(); ++i)
                        {
                            const YAML::Node runtime = packages[i]["runtime"];
                            if (runtime && runtime["event_binding"])
                            {
                                return runtime["event_binding"].as<std::string>();
                            }
                        }
                    }

                    YAML::Node runtime = autosar["runtime"];
                    if (runtime && runtime["event_binding"])
                    {
                        return runtime["event_binding"].as<std::string>();
                    }
                }
            }
            catch (...)
            {
                return std::string{};
            }

            return std::string{};
        }

        inline EventTransportBinding ResolveEventTransportBinding(
            const std::string &deploymentHintToken = std::string{})
        {
            EventTransportBinding parsedBinding{EventTransportBinding::kDds};
            bool isAuto{false};

            const char *envBinding = std::getenv("ARA_COM_EVENT_BINDING");
            if (envBinding != nullptr &&
                ParseTransportBindingToken(envBinding, parsedBinding, &isAuto))
            {
                return isAuto ? ResolveAutoEventTransportBinding()
                              : parsedBinding;
            }

            const std::string manifestToken = ResolveManifestEventBindingToken();
            if (!manifestToken.empty() &&
                ParseTransportBindingToken(manifestToken, parsedBinding, &isAuto))
            {
                return isAuto ? ResolveAutoEventTransportBinding()
                              : parsedBinding;
            }

            if (!deploymentHintToken.empty() &&
                ParseTransportBindingToken(deploymentHintToken, parsedBinding, &isAuto))
            {
                return isAuto ? ResolveAutoEventTransportBinding()
                              : parsedBinding;
            }

            return EventTransportBinding::kDds;
        }

        struct EventInstanceDeployment
        {
            std::string instance_specifier;
            std::string event_binding{"auto"};
            std::uint16_t service_interface_id{0x0000U};
            std::uint16_t service_instance_id{0x0001U};
            std::uint16_t event_group_id{0x0001U};
            std::uint16_t event_id{0x8001U};
            std::uint8_t major_version{1U};
            std::uint32_t minor_version{0U};
            std::uint32_t dds_domain_id{0U};
            std::string iceoryx_service;
            std::string iceoryx_instance;
            std::string iceoryx_event;
            std::string iceoryx_runtime_name{"adaptive_autosar_ara_com"};
            std::uint64_t iceoryx_history_capacity{0U};
            std::uint64_t iceoryx_queue_capacity{256U};
            std::uint64_t iceoryx_history_request{0U};
        };

        struct ResolvedEventBinding
        {
            bool has_mapping{false};
            std::string input_topic;
            std::string ros_topic;
            std::string dds_topic_name;
            EventInstanceDeployment deployment;
        };

        inline ara::core::InstanceSpecifier CreateInstanceSpecifierOrThrow(const std::string &path)
        {
            auto result = ara::core::InstanceSpecifier::Create(path.empty() ? std::string{} : path);
            if (result.HasValue())
            {
                return result.Value();
            }

            auto fallback = ara::core::InstanceSpecifier::Create("/ara/com/generated/default");
            if (fallback.HasValue())
            {
                return fallback.Value();
            }

            return ara::core::InstanceSpecifier::Create("/ara/com/generated/fallback").Value();
        }

        namespace detail
        {
            inline std::string Trim(const std::string &value)
            {
                auto begin = value.begin();
                auto end = value.end();
                while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
                {
                    ++begin;
                }
                while (begin != end)
                {
                    auto last = end;
                    --last;
                    if (!std::isspace(static_cast<unsigned char>(*last)))
                    {
                        break;
                    }
                    end = last;
                }
                return std::string(begin, end);
            }

            inline bool FileExists(const std::string &path)
            {
                std::ifstream ifs(path.c_str());
                return ifs.good();
            }

            inline std::uint32_t ParseU32(const YAML::Node &node, std::uint32_t fallback)
            {
                if (!node || node.IsNull())
                {
                    return fallback;
                }

                try
                {
                    if (node.IsScalar())
                    {
                        const std::string text = Trim(node.as<std::string>());
                        if (text.empty())
                        {
                            return fallback;
                        }
                        std::size_t idx = 0U;
                        const unsigned long parsed = std::stoul(text, &idx, 0);
                        if (idx == text.size())
                        {
                            return static_cast<std::uint32_t>(parsed);
                        }
                    }
                    return node.as<std::uint32_t>();
                }
                catch (...)
                {
                    return fallback;
                }
            }

            inline std::uint64_t ParseU64(const YAML::Node &node, std::uint64_t fallback)
            {
                if (!node || node.IsNull())
                {
                    return fallback;
                }

                try
                {
                    if (node.IsScalar())
                    {
                        const std::string text = Trim(node.as<std::string>());
                        if (text.empty())
                        {
                            return fallback;
                        }
                        std::size_t idx = 0U;
                        const unsigned long long parsed =
                            std::stoull(text, &idx, 0);
                        if (idx == text.size())
                        {
                            return static_cast<std::uint64_t>(parsed);
                        }
                    }
                    return node.as<std::uint64_t>();
                }
                catch (...)
                {
                    return fallback;
                }
            }

            inline std::string NormalizeRosTopic(const std::string &topic_name)
            {
                std::string topic = Trim(topic_name);
                if (topic.empty())
                {
                    return topic;
                }

                if (topic.rfind("rt/", 0U) == 0U || topic.rfind("rp/", 0U) == 0U)
                {
                    return "/" + topic.substr(3U);
                }

                if (topic[0] == '/')
                {
                    return topic;
                }

                return "/" + topic;
            }

            inline std::string NormalizeRtTopic(const std::string &topic_name)
            {
                std::string topic = Trim(topic_name);
                if (topic.empty())
                {
                    return topic;
                }

                if (topic.rfind("rt/", 0U) == 0U || topic.rfind("rp/", 0U) == 0U)
                {
                    return topic;
                }

                if (topic[0] == '/')
                {
                    return "rt" + topic;
                }

                return "rt/" + topic;
            }

            inline std::string SanitizeZeroCopyToken(std::string token)
            {
                std::transform(
                    token.begin(),
                    token.end(),
                    token.begin(),
                    [](unsigned char c)
                    {
                        if (std::isalnum(c) != 0)
                        {
                            return static_cast<char>(c);
                        }
                        return '_';
                    });

                token.erase(
                    token.begin(),
                    std::find_if(
                        token.begin(),
                        token.end(),
                        [](char c)
                        {
                            return c != '_';
                        }));

                while (!token.empty() && token.back() == '_')
                {
                    token.pop_back();
                }

                if (token.empty())
                {
                    return std::string{"default"};
                }
                return token;
            }

            inline ara::com::zerocopy::ChannelDescriptor
            BuildZeroCopyChannelDescriptor(
                const ResolvedEventBinding &binding,
                const std::string &fallbackTopicName)
            {
                ara::com::zerocopy::ChannelDescriptor channel{};
                channel.Service = Trim(binding.deployment.iceoryx_service);
                channel.Instance = Trim(binding.deployment.iceoryx_instance);
                channel.Event = Trim(binding.deployment.iceoryx_event);

                if (!channel.Service.empty() &&
                    !channel.Instance.empty() &&
                    !channel.Event.empty())
                {
                    return channel;
                }

                std::string topic = Trim(binding.dds_topic_name);
                if (topic.empty())
                {
                    topic = Trim(fallbackTopicName);
                }
                if (topic.rfind("rt/", 0U) == 0U ||
                    topic.rfind("rp/", 0U) == 0U)
                {
                    topic = topic.substr(3U);
                }
                else if (!topic.empty() && topic.front() == '/')
                {
                    topic = topic.substr(1U);
                }

                std::vector<std::string> segments;
                std::size_t begin = 0U;
                while (begin < topic.size())
                {
                    const std::size_t end = topic.find('/', begin);
                    const std::string segment = topic.substr(
                        begin,
                        end == std::string::npos
                            ? std::string::npos
                            : (end - begin));
                    if (!segment.empty())
                    {
                        segments.push_back(
                            SanitizeZeroCopyToken(segment));
                    }
                    if (end == std::string::npos)
                    {
                        break;
                    }
                    begin = end + 1U;
                }

                if (channel.Service.empty())
                {
                    channel.Service = segments.size() > 0U
                                          ? segments[0]
                                          : std::string{"autosar"};
                }
                if (channel.Instance.empty())
                {
                    channel.Instance = segments.size() > 1U
                                           ? segments[1]
                                           : std::string{"adaptive"};
                }
                if (channel.Event.empty())
                {
                    if (segments.size() > 2U)
                    {
                        channel.Event = segments[2];
                        for (std::size_t i = 3U; i < segments.size(); ++i)
                        {
                            channel.Event += "_" + segments[i];
                        }
                    }
                    else
                    {
                        channel.Event = std::string{"status"};
                    }
                }

                return channel;
            }

            class EventBindingRegistry
            {
            public:
                static EventBindingRegistry &Instance()
                {
                    static EventBindingRegistry registry;
                    return registry;
                }

                ResolvedEventBinding Resolve(const std::string &topic_name)
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    EnsureLoadedLocked();

                    const std::string trimmed = Trim(topic_name);
                    if (trimmed.empty())
                    {
                        throw std::runtime_error("Topic name must not be empty.");
                    }

                    const std::string normalized_ros = NormalizeRosTopic(trimmed);
                    const std::string normalized_rt = NormalizeRtTopic(trimmed);

                    auto it = bindings_.find(trimmed);
                    if (it == bindings_.end())
                    {
                        it = bindings_.find(normalized_ros);
                    }
                    if (it == bindings_.end())
                    {
                        it = bindings_.find(normalized_rt);
                    }

                    if (it != bindings_.end())
                    {
                        return it->second;
                    }

                    if (require_mapping_)
                    {
                        throw std::runtime_error(
                            "AUTOSAR topic mapping entry not found for: " + trimmed);
                    }

                        ResolvedEventBinding fallback;
                        fallback.has_mapping = false;
                        fallback.input_topic = trimmed;
                        fallback.ros_topic = normalized_ros;
                        fallback.dds_topic_name = trimmed;
                        fallback.deployment.event_binding = "auto";
                        return fallback;
                    }

            private:
                EventBindingRegistry() = default;

                void AddAlias(const std::string &key, const ResolvedEventBinding &binding)
                {
                    const std::string alias = Trim(key);
                    if (!alias.empty())
                    {
                        bindings_[alias] = binding;
                    }
                }

                void RegisterBinding(const ResolvedEventBinding &binding)
                {
                    ResolvedEventBinding normalized = binding;
                    normalized.ros_topic = NormalizeRosTopic(
                        binding.ros_topic.empty() ? binding.input_topic : binding.ros_topic);

                    if (binding.dds_topic_name.empty())
                    {
                        normalized.dds_topic_name = NormalizeRtTopic(normalized.ros_topic);
                    }
                    else
                    {
                        normalized.dds_topic_name = Trim(binding.dds_topic_name);
                    }

                    AddAlias(binding.input_topic, normalized);
                    AddAlias(normalized.ros_topic, normalized);
                    AddAlias(normalized.dds_topic_name, normalized);
                    AddAlias(NormalizeRtTopic(normalized.ros_topic), normalized);
                    AddAlias(NormalizeRosTopic(normalized.dds_topic_name), normalized);
                }

                bool LoadFromFile(const std::string &path)
                {
                    YAML::Node root;
                    try
                    {
                        root = YAML::LoadFile(path);
                    }
                    catch (...)
                    {
                        return false;
                    }

                    YAML::Node mappings = root["topic_mappings"];
                    if (!mappings || !mappings.IsSequence())
                    {
                        return false;
                    }

                    for (std::size_t i = 0U; i < mappings.size(); ++i)
                    {
                        const YAML::Node &entry = mappings[i];
                        if (!entry || !entry.IsMap())
                        {
                            continue;
                        }

                        ResolvedEventBinding binding;
                        binding.has_mapping = true;
                        binding.input_topic = entry["ros_topic"] ? entry["ros_topic"].as<std::string>() : "";
                        binding.ros_topic = binding.input_topic;
                        binding.dds_topic_name =
                            entry["dds_topic"] ? entry["dds_topic"].as<std::string>() : "";

                        const YAML::Node ara = entry["ara"];
                        if (ara && ara.IsMap())
                        {
                            binding.deployment.instance_specifier =
                                ara["instance_specifier"] ? ara["instance_specifier"].as<std::string>() : "";
                            binding.deployment.event_binding =
                                ara["event_binding"] ? ara["event_binding"].as<std::string>() : "auto";
                            binding.deployment.service_interface_id =
                                static_cast<std::uint16_t>(ParseU32(ara["service_interface_id"], 0U));
                            binding.deployment.service_instance_id =
                                static_cast<std::uint16_t>(ParseU32(ara["service_instance_id"], 1U));
                            binding.deployment.event_group_id =
                                static_cast<std::uint16_t>(ParseU32(ara["event_group_id"], 1U));
                            binding.deployment.event_id =
                                static_cast<std::uint16_t>(ParseU32(ara["event_id"], 0x8001U));
                            binding.deployment.major_version =
                                static_cast<std::uint8_t>(ParseU32(ara["major_version"], 1U));
                            binding.deployment.minor_version = ParseU32(ara["minor_version"], 0U);
                            binding.deployment.dds_domain_id =
                                ParseU32(ara["dds_domain_id"], 0U);
                            binding.deployment.iceoryx_service =
                                ara["iceoryx_service"] ? ara["iceoryx_service"].as<std::string>() : "";
                            binding.deployment.iceoryx_instance =
                                ara["iceoryx_instance"] ? ara["iceoryx_instance"].as<std::string>() : "";
                            binding.deployment.iceoryx_event =
                                ara["iceoryx_event"] ? ara["iceoryx_event"].as<std::string>() : "";
                            binding.deployment.iceoryx_runtime_name =
                                ara["iceoryx_runtime_name"] ? ara["iceoryx_runtime_name"].as<std::string>() : "adaptive_autosar_ara_com";
                            binding.deployment.iceoryx_history_capacity =
                                ParseU64(ara["iceoryx_history_capacity"], 0U);
                            binding.deployment.iceoryx_queue_capacity =
                                ParseU64(ara["iceoryx_queue_capacity"], 256U);
                            binding.deployment.iceoryx_history_request =
                                ParseU64(ara["iceoryx_history_request"], 0U);
                        }
                        else
                        {
                            binding.deployment.event_binding = "auto";
                        }

                        if (binding.deployment.instance_specifier.empty())
                        {
                            binding.deployment.instance_specifier = "/ara/com/generated/default";
                        }
                        binding.deployment.event_binding = NormalizeTransportName(
                            binding.deployment.event_binding);
                        if (binding.deployment.event_binding.empty())
                        {
                            binding.deployment.event_binding = "auto";
                        }
                        if (binding.deployment.iceoryx_runtime_name.empty())
                        {
                            binding.deployment.iceoryx_runtime_name = "adaptive_autosar_ara_com";
                        }

                        if (binding.input_topic.empty())
                        {
                            binding.input_topic = NormalizeRosTopic(binding.dds_topic_name);
                        }
                        if (binding.input_topic.empty() && !binding.dds_topic_name.empty())
                        {
                            binding.input_topic = binding.dds_topic_name;
                        }
                        if (binding.input_topic.empty())
                        {
                            continue;
                        }

                        RegisterBinding(binding);
                    }

                    return !bindings_.empty();
                }

                static std::vector<std::string> CandidatePaths(const char *override_path)
                {
                    std::vector<std::string> paths;
                    if (override_path != nullptr && *override_path != 0)
                    {
                        paths.emplace_back(override_path);
                    }

                    paths.emplace_back("./autosar_topic_mapping.yaml");
                    paths.emplace_back("./autosar/autosar_topic_mapping.yaml");
                    paths.emplace_back("./apps/build-adaptive-autosar/autosar/autosar_topic_mapping.yaml");
                    paths.emplace_back("/opt/autosar-ap-libs/share/autosar/com/autosar_topic_mapping.yaml");
                    paths.emplace_back("/opt/autosar_ap/configuration/autosar_topic_mapping.yaml");
                    return paths;
                }

                void EnsureLoadedLocked()
                {
                    if (loaded_)
                    {
                        return;
                    }

                    disable_mapping_ =
                        ParseBoolEnv("ARA_COM_DISABLE_TOPIC_MAPPING");
                    require_mapping_ =
                        ParseBoolEnv("ARA_COM_REQUIRE_TOPIC_MAPPING");
                    loaded_ = true;

                    if (disable_mapping_)
                    {
                        return;
                    }

                    const char *override_path = std::getenv("ARA_COM_TOPIC_MAPPING");

                    const std::vector<std::string> candidates = CandidatePaths(override_path);
                    for (std::size_t i = 0U; i < candidates.size(); ++i)
                    {
                        if (!FileExists(candidates[i]))
                        {
                            continue;
                        }

                        if (LoadFromFile(candidates[i]))
                        {
                            loaded_mapping_path_ = candidates[i];
                            return;
                        }
                    }
                }

                std::unordered_map<std::string, ResolvedEventBinding> bindings_;
                std::mutex mutex_;
                bool loaded_{false};
                bool disable_mapping_{false};
                bool require_mapping_{false};
                std::string loaded_mapping_path_;
            };

            template <typename T>
            std::vector<std::uint8_t> SerializeSample(const T &message)
            {
                using namespace org::eclipse::cyclonedds::core::cdr;

                T mutable_message = message;
                basic_cdr_stream sizer;
                move(sizer, mutable_message, false);
                const std::size_t payload_size = sizer.position();

                std::vector<std::uint8_t> output(payload_size + 4U, 0U);
                output[0] = 0x00;
                output[1] = 0x01;
                output[2] = 0x00;
                output[3] = 0x00;

                basic_cdr_stream writer;
                writer.set_buffer(reinterpret_cast<char *>(output.data() + 4U), payload_size);
                write(writer, mutable_message, false);
                return output;
            }

            template <typename T>
            bool DeserializeSample(const std::vector<std::uint8_t> &payload, T &message)
            {
                using namespace org::eclipse::cyclonedds::core::cdr;
                if (payload.size() <= 4U)
                {
                    return false;
                }

                basic_cdr_stream reader;
                reader.set_buffer(
                    reinterpret_cast<char *>(const_cast<std::uint8_t *>(payload.data() + 4U)),
                    payload.size() - 4U);
                read(reader, message, false);
                return true;
            }
        } // namespace detail

        class SomeipEventSkeletonAdapter : public ara::com::ServiceSkeletonBase
        {
        public:
            ara::com::SkeletonEvent<std::vector<std::uint8_t>> Event;

            explicit SomeipEventSkeletonAdapter(const EventInstanceDeployment &deployment)
                : ara::com::ServiceSkeletonBase(
                      CreateInstanceSpecifierOrThrow(deployment.instance_specifier),
                      deployment.service_interface_id,
                      deployment.service_instance_id,
                      deployment.major_version,
                      deployment.minor_version,
                      ara::com::MethodCallProcessingMode::kEvent),
                  Event(
                      ara::com::internal::BindingFactory::CreateSkeletonEventBinding(
                          ara::com::internal::TransportBinding::kVsomeip,
                          ara::com::internal::EventBindingConfig{
                              deployment.service_interface_id,
                              deployment.service_instance_id,
                              deployment.event_id,
                              deployment.event_group_id,
                              deployment.major_version}))
            {
            }

            ara::core::Result<void> OfferEventService()
            {
                auto offer_service = OfferService();
                if (!offer_service.HasValue())
                {
                    return offer_service;
                }
                return Event.Offer();
            }

            void StopEventService()
            {
                Event.StopOffer();
                StopOfferService();
            }
        };

        class SomeipEventProxyAdapter : public ara::com::ServiceProxyBase
        {
        public:
            using HandleType = ara::com::ServiceHandleType;
            ara::com::ProxyEvent<std::vector<std::uint8_t>> Event;

            explicit SomeipEventProxyAdapter(const EventInstanceDeployment &deployment)
                : ara::com::ServiceProxyBase(
                      HandleType{
                          deployment.service_interface_id,
                          deployment.service_instance_id}),
                  Event(
                      ara::com::internal::BindingFactory::CreateProxyEventBinding(
                          ara::com::internal::TransportBinding::kVsomeip,
                          ara::com::internal::EventBindingConfig{
                              deployment.service_interface_id,
                              deployment.service_instance_id,
                              deployment.event_id,
                              deployment.event_group_id,
                              deployment.major_version}))
            {
            }
        };

        template <typename SampleType>
        class EventPublisherAdapter
        {
        public:
            EventPublisherAdapter(
                const std::string &topic_name,
                std::uint32_t domain_id)
                : resolved_binding_(detail::EventBindingRegistry::Instance().Resolve(topic_name)),
                  binding_(ResolveEventTransportBinding(
                      resolved_binding_.deployment.event_binding)),
                  dds_publisher_(nullptr),
                  zerocopy_publisher_(nullptr),
                  someip_skeleton_(nullptr),
                  someip_subscriber_count_(0)
            {
                const std::string dds_topic_name =
                    resolved_binding_.dds_topic_name.empty()
                        ? topic_name
                        : resolved_binding_.dds_topic_name;
                const std::uint32_t resolved_dds_domain_id{
                    resolved_binding_.deployment.dds_domain_id == 0U
                        ? domain_id
                        : resolved_binding_.deployment.dds_domain_id};

                if (binding_ == EventTransportBinding::kSomeip && resolved_binding_.has_mapping)
                {
                    someip_skeleton_ = std::make_unique<SomeipEventSkeletonAdapter>(
                        resolved_binding_.deployment);

                    auto offer = someip_skeleton_->OfferEventService();
                    if (!offer.HasValue())
                    {
                        throw std::runtime_error("Failed to offer SOME/IP event service.");
                    }

                    (void)someip_skeleton_->SetEventSubscriptionStateHandler(
                        resolved_binding_.deployment.event_group_id,
                        [this](std::uint16_t, bool subscribed) -> bool
                        {
                            if (subscribed)
                            {
                                someip_subscriber_count_.fetch_add(1);
                            }
                            else
                            {
                                const auto current = someip_subscriber_count_.load();
                                if (current > 0)
                                {
                                    someip_subscriber_count_.fetch_sub(1);
                                }
                            }
                            return true;
                        });
                    return;
                }

                if (binding_ == EventTransportBinding::kIceoryx)
                {
                    const auto channel = detail::BuildZeroCopyChannelDescriptor(
                        resolved_binding_,
                        topic_name);
                    zerocopy_publisher_ = std::make_unique<
                        ara::com::zerocopy::ZeroCopyPublisher>(
                        channel,
                        resolved_binding_.deployment.iceoryx_runtime_name,
                        resolved_binding_.deployment.iceoryx_history_capacity);
                    if (!zerocopy_publisher_->IsBindingActive())
                    {
                        throw std::runtime_error(
                            "ara::com iceoryx publisher binding is not active.");
                    }
                    return;
                }

                binding_ = EventTransportBinding::kDds;
                dds_publisher_ = std::make_unique<ara::com::dds::DdsPublisher<SampleType>>(
                    dds_topic_name,
                    resolved_dds_domain_id);

                if (!dds_publisher_->IsBindingActive())
                {
                    throw std::runtime_error("ara::com DDS publisher binding is not active.");
                }
            }

            ~EventPublisherAdapter()
            {
                if (someip_skeleton_)
                {
                    if (resolved_binding_.has_mapping)
                    {
                        someip_skeleton_->UnsetEventSubscriptionStateHandler(
                            resolved_binding_.deployment.event_group_id);
                    }
                    someip_skeleton_->StopEventService();
                }
            }

            EventPublisherAdapter(const EventPublisherAdapter &) = delete;
            EventPublisherAdapter &operator=(const EventPublisherAdapter &) = delete;

            EventTransportBinding GetBinding() const noexcept
            {
                return binding_;
            }

            std::string GetResolvedDdsTopicName() const
            {
                return resolved_binding_.dds_topic_name.empty()
                           ? resolved_binding_.input_topic
                           : resolved_binding_.dds_topic_name;
            }

            bool IsArxmlMapped() const noexcept
            {
                return resolved_binding_.has_mapping;
            }

            void Publish(const SampleType &sample) const
            {
                if (binding_ == EventTransportBinding::kDds)
                {
                    if (dds_publisher_)
                    {
                        dds_publisher_->Write(sample);
                    }
                    return;
                }

                if (binding_ == EventTransportBinding::kSomeip &&
                    someip_skeleton_)
                {
                    someip_skeleton_->Event.Send(detail::SerializeSample(sample));
                    return;
                }

                if (binding_ == EventTransportBinding::kIceoryx &&
                    zerocopy_publisher_)
                {
                    auto payload = ara::com::Serializer<SampleType>::Serialize(
                        sample);
                    (void)zerocopy_publisher_->PublishCopy(payload);
                }
            }

            std::int32_t GetMatchedSubscriptionCount() const
            {
                if (binding_ == EventTransportBinding::kDds)
                {
                    if (dds_publisher_)
                    {
                        auto result = dds_publisher_->GetMatchedSubscriptionCount();
                        if (result.HasValue())
                        {
                            return result.Value();
                        }
                    }
                    return 0;
                }

                if (binding_ == EventTransportBinding::kIceoryx)
                {
                    if (zerocopy_publisher_ &&
                        zerocopy_publisher_->HasSubscribers())
                    {
                        return 1;
                    }
                    return 0;
                }

                return someip_subscriber_count_.load();
            }

        private:
            ResolvedEventBinding resolved_binding_;
            EventTransportBinding binding_;
            std::unique_ptr<ara::com::dds::DdsPublisher<SampleType>> dds_publisher_;
            std::unique_ptr<ara::com::zerocopy::ZeroCopyPublisher> zerocopy_publisher_;
            std::unique_ptr<SomeipEventSkeletonAdapter> someip_skeleton_;
            mutable std::atomic<std::int32_t> someip_subscriber_count_;
        };

        template <typename SampleType>
        class EventSubscriberAdapter
        {
        public:
            using SampleHandler = std::function<void(const SampleType &)>;

            EventSubscriberAdapter(
                const std::string &topic_name,
                std::uint32_t domain_id,
                std::size_t someip_queue_size)
                : resolved_binding_(detail::EventBindingRegistry::Instance().Resolve(topic_name)),
                  binding_(ResolveEventTransportBinding(
                      resolved_binding_.deployment.event_binding)),
                  dds_subscriber_(nullptr),
                  zerocopy_subscriber_(nullptr),
                  someip_proxy_(nullptr),
                  someip_queue_size_(someip_queue_size),
                  zerocopy_has_publication_(0)
            {
                const std::string dds_topic_name =
                    resolved_binding_.dds_topic_name.empty()
                        ? topic_name
                        : resolved_binding_.dds_topic_name;
                const std::uint32_t resolved_dds_domain_id{
                    resolved_binding_.deployment.dds_domain_id == 0U
                        ? domain_id
                        : resolved_binding_.deployment.dds_domain_id};

                if (binding_ == EventTransportBinding::kSomeip && resolved_binding_.has_mapping)
                {
                    someip_proxy_ = std::make_unique<SomeipEventProxyAdapter>(
                        resolved_binding_.deployment);
                    someip_proxy_->Event.Subscribe(
                        static_cast<std::uint32_t>(someip_queue_size_));
                    return;
                }

                if (binding_ == EventTransportBinding::kIceoryx)
                {
                    const auto channel = detail::BuildZeroCopyChannelDescriptor(
                        resolved_binding_,
                        topic_name);
                    const std::uint64_t queue_capacity{
                        resolved_binding_.deployment.iceoryx_queue_capacity == 0U
                            ? static_cast<std::uint64_t>(
                                  std::max<std::size_t>(someip_queue_size_, 1U))
                            : resolved_binding_.deployment.iceoryx_queue_capacity};

                    zerocopy_subscriber_ = std::make_unique<
                        ara::com::zerocopy::ZeroCopySubscriber>(
                        channel,
                        resolved_binding_.deployment.iceoryx_runtime_name,
                        queue_capacity,
                        resolved_binding_.deployment.iceoryx_history_request);
                    if (!zerocopy_subscriber_->IsBindingActive())
                    {
                        throw std::runtime_error(
                            "ara::com iceoryx subscriber binding is not active.");
                    }
                    return;
                }

                binding_ = EventTransportBinding::kDds;
                dds_subscriber_ = std::make_unique<ara::com::dds::DdsSubscriber<SampleType>>(
                    dds_topic_name,
                    resolved_dds_domain_id);

                if (!dds_subscriber_->IsBindingActive())
                {
                    throw std::runtime_error("ara::com DDS subscriber binding is not active.");
                }
            }

            ~EventSubscriberAdapter()
            {
                Stop();
            }

            EventSubscriberAdapter(const EventSubscriberAdapter &) = delete;
            EventSubscriberAdapter &operator=(const EventSubscriberAdapter &) = delete;

            EventTransportBinding GetBinding() const noexcept
            {
                return binding_;
            }

            std::string GetResolvedDdsTopicName() const
            {
                return resolved_binding_.dds_topic_name.empty()
                           ? resolved_binding_.input_topic
                           : resolved_binding_.dds_topic_name;
            }

            bool IsArxmlMapped() const noexcept
            {
                return resolved_binding_.has_mapping;
            }

            void Poll(std::uint32_t max_samples, const SampleHandler &handler)
            {
                if (!handler)
                {
                    return;
                }

                if (binding_ == EventTransportBinding::kDds)
                {
                    if (!dds_subscriber_)
                    {
                        return;
                    }

                    (void)dds_subscriber_->Take(
                        max_samples,
                        [&handler](const SampleType &sample)
                        {
                            handler(sample);
                        });
                    return;
                }

                if (binding_ == EventTransportBinding::kSomeip)
                {
                    if (!someip_proxy_)
                    {
                        return;
                    }

                    (void)someip_proxy_->Event.GetNewSamples(
                        [&handler](ara::com::SamplePtr<std::vector<std::uint8_t>> payload_sample)
                        {
                            if (!payload_sample)
                            {
                                return;
                            }

                            SampleType sample{};
                            if (detail::DeserializeSample(*payload_sample, sample))
                            {
                                handler(sample);
                            }
                        },
                        max_samples);
                    return;
                }

                if (binding_ == EventTransportBinding::kIceoryx)
                {
                    if (!zerocopy_subscriber_)
                    {
                        return;
                    }

                    const std::size_t sample_budget{
                        std::max<std::size_t>(
                            static_cast<std::size_t>(1U),
                            static_cast<std::size_t>(max_samples))};
                    std::size_t received_count{0U};
                    for (std::size_t i = 0U; i < sample_budget; ++i)
                    {
                        ara::com::zerocopy::ReceivedSample sample;
                        auto take_result = zerocopy_subscriber_->TryTake(sample);
                        if (!take_result.HasValue() || !take_result.Value())
                        {
                            break;
                        }

                        if (sample.Size() == 0U)
                        {
                            continue;
                        }

                        auto deserialize_result = ara::com::Serializer<SampleType>::Deserialize(
                            sample.Data(),
                            sample.Size());
                        if (deserialize_result.HasValue())
                        {
                            handler(deserialize_result.Value());
                            ++received_count;
                        }
                    }

                    if (received_count > 0U)
                    {
                        zerocopy_has_publication_.store(1);
                    }
                    return;
                }
            }

            std::int32_t GetMatchedPublicationCount() const
            {
                if (binding_ == EventTransportBinding::kDds)
                {
                    if (dds_subscriber_)
                    {
                        auto result = dds_subscriber_->GetMatchedPublicationCount();
                        if (result.HasValue())
                        {
                            return result.Value();
                        }
                    }
                    return 0;
                }

                if (binding_ == EventTransportBinding::kIceoryx)
                {
                    return zerocopy_has_publication_.load();
                }

                if (!someip_proxy_)
                {
                    return 0;
                }

                const auto state = someip_proxy_->Event.GetSubscriptionState();
                return state == ara::com::SubscriptionState::kSubscribed ? 1 : 0;
            }

            void Stop()
            {
                if (binding_ == EventTransportBinding::kSomeip &&
                    someip_proxy_)
                {
                    someip_proxy_->Event.UnsetReceiveHandler();
                    someip_proxy_->Event.Unsubscribe();
                }
            }

        private:
            ResolvedEventBinding resolved_binding_;
            EventTransportBinding binding_;
            std::unique_ptr<ara::com::dds::DdsSubscriber<SampleType>> dds_subscriber_;
            std::unique_ptr<ara::com::zerocopy::ZeroCopySubscriber> zerocopy_subscriber_;
            std::unique_ptr<SomeipEventProxyAdapter> someip_proxy_;
            std::size_t someip_queue_size_;
            mutable std::atomic<std::int32_t> zerocopy_has_publication_;
        };
    } // namespace com
} // namespace ara

#endif // ARA_COM_EVENT_BINDING_ADAPTER_H
