#!/usr/bin/env python3
"""Generate Adaptive AUTOSAR topic mapping + manifest from app sources.

This script scans app C++ sources for ROS-style communication API calls:
  - create_publisher<MsgType>(topic, ...)
  - create_subscription<MsgType>(topic, ...)
  - create_service<SrvType>(service_name, ...)
  - create_client<SrvType>(service_name)

From discovered topic/service usage, it generates:
  1) topic mapping YAML(JSON-compatible) for AUTOSAR runtime
  2) ARXML generator input manifest YAML(JSON-compatible)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}

RE_PUBLISHER = re.compile(
    r"create_publisher\s*<\s*([^>]+?)\s*>\s*\(\s*(.+?)\s*,",
    re.DOTALL,
)
RE_SUBSCRIPTION = re.compile(
    r"create_subscription\s*<\s*([^>]+?)\s*>\s*\(\s*(.+?)\s*,",
    re.DOTALL,
)
RE_SERVICE = re.compile(
    r"create_service\s*<\s*([^>]+?)\s*>\s*\(\s*(.+?)\s*,",
    re.DOTALL,
)
RE_CLIENT = re.compile(
    r"create_client\s*<\s*([^>]+?)\s*>\s*\(\s*(.+?)\s*\)",
    re.DOTALL,
)

# Best-effort variable literal extraction
RE_VAR_ASSIGN = re.compile(
    r"\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*=\s*\"((?:\\.|[^\"\\])*)\"",
    re.DOTALL,
)
RE_VAR_INIT = re.compile(
    r"\b([A-Za-z_]\w*)\s*\(\s*\"((?:\\.|[^\"\\])*)\"\s*\)",
    re.DOTALL,
)
RE_STRING = re.compile(r"\"((?:\\.|[^\"\\])*)\"")
RE_IDENTIFIER = re.compile(r"^[A-Za-z_]\w*$")


def decode_cpp_string(value: str) -> str:
    # Minimal decoding that handles common escapes in topic literals.
    return bytes(value, "utf-8").decode("unicode_escape")


def sanitize_type_name(type_name: str) -> str:
    return re.sub(r"\s+", "", type_name.strip())


def normalize_ros_topic(topic: str) -> str:
    text = topic.strip()
    if not text:
        return text
    if text.startswith("rt/") or text.startswith("rp/"):
        text = "/" + text[3:]
    if not text.startswith("/"):
        text = "/" + text
    return text


def ros_topic_to_dds_topic(ros_topic: str) -> str:
    normalized = normalize_ros_topic(ros_topic)
    return "rt" + normalized


def normalize_service_name(service_name: str) -> str:
    text = service_name.strip()
    if not text:
        return text
    if text.startswith("rp/"):
        text = "/" + text[3:]
    if not text.startswith("/"):
        text = "/" + text
    return text


def service_to_request_topic(service_name: str) -> Tuple[str, str]:
    service_base = normalize_service_name(service_name).lstrip("/")
    dds_topic = f"rp/{service_base}_Request"
    ros_topic = "/" + dds_topic[3:]
    return ros_topic, dds_topic


def service_to_response_topic(service_name: str) -> Tuple[str, str]:
    service_base = normalize_service_name(service_name).lstrip("/")
    dds_topic = f"rp/{service_base}_Response"
    ros_topic = "/" + dds_topic[3:]
    return ros_topic, dds_topic


def to_identifier(value: str) -> str:
    token = re.sub(r"[^A-Za-z0-9]+", "_", value)
    token = re.sub(r"_+", "_", token).strip("_")
    if not token:
        token = "X"
    if token[0].isdigit():
        token = "X_" + token
    return token


def allocate_u16(base: int, span: int, key: str, used: Set[int]) -> int:
    start = zlib.crc32(key.encode("utf-8")) % span
    for i in range(span):
        candidate = base + ((start + i) % span)
        if candidate not in used:
            used.add(candidate)
            return candidate
    raise RuntimeError(f"No available ID in range base=0x{base:04X}, span={span}")


def format_hex(value: int) -> str:
    return f"0x{value:04X}"


VALID_EVENT_BINDINGS = {"auto", "dds", "vsomeip"}


def normalize_event_binding(value: str) -> str:
    binding = value.strip().lower()
    if binding not in VALID_EVENT_BINDINGS:
        raise ValueError(
            "Unsupported event binding '{}'. Expected one of: {}".format(
                value,
                ", ".join(sorted(VALID_EVENT_BINDINGS)),
            )
        )
    return binding


@dataclass(frozen=True)
class MsgUsage:
    type_name: str
    ros_topic: str


@dataclass(frozen=True)
class SrvUsage:
    type_name: str
    ros_service: str


def list_source_files(apps_root: Path) -> List[Path]:
    files: List[Path] = []
    for path in apps_root.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in CPP_SUFFIXES:
            continue
        rel = path.relative_to(apps_root)
        parts = set(rel.parts)
        if any(
            part.startswith("build")
            or part.startswith("install")
            or part in {"generated", ".git"}
            for part in parts
        ):
            continue
        files.append(path)
    return sorted(files)


def extract_variable_literals(source: str) -> Dict[str, str]:
    literals: Dict[str, str] = {}
    for match in RE_VAR_ASSIGN.finditer(source):
        name, value = match.group(1), match.group(2)
        literals[name] = decode_cpp_string(value)
    for match in RE_VAR_INIT.finditer(source):
        name, value = match.group(1), match.group(2)
        # Prefer explicit assignments if both exist.
        literals.setdefault(name, decode_cpp_string(value))
    return literals


def normalize_expr(expr: str) -> str:
    text = expr.strip()
    text = re.sub(r"\s+", " ", text)
    return text


def resolve_string_expr(expr: str, literals: Dict[str, str]) -> Optional[str]:
    text = normalize_expr(expr)
    if not text:
        return None

    # Direct "..."
    m = RE_STRING.search(text)
    if m:
        return decode_cpp_string(m.group(1))

    # this->var, var, var.c_str()
    token = text.replace("this->", "").strip()
    token = re.sub(r"\.c_str\(\)\s*$", "", token)
    token = re.sub(r"\.data\(\)\s*$", "", token)

    if RE_IDENTIFIER.match(token):
        return literals.get(token)

    return None


def extract_calls(source: str) -> Dict[str, List[Tuple[str, str]]]:
    results = {
        "publisher": [],
        "subscription": [],
        "service": [],
        "client": [],
    }
    for match in RE_PUBLISHER.finditer(source):
        results["publisher"].append((sanitize_type_name(match.group(1)), match.group(2)))
    for match in RE_SUBSCRIPTION.finditer(source):
        results["subscription"].append((sanitize_type_name(match.group(1)), match.group(2)))
    for match in RE_SERVICE.finditer(source):
        results["service"].append((sanitize_type_name(match.group(1)), match.group(2)))
    for match in RE_CLIENT.finditer(source):
        results["client"].append((sanitize_type_name(match.group(1)), match.group(2)))
    return results


def discover_usage(
    apps_root: Path,
) -> Tuple[List[MsgUsage], List[SrvUsage], List[str]]:
    msg_set: Set[MsgUsage] = set()
    srv_set: Set[SrvUsage] = set()
    warnings: List[str] = []

    for source_path in list_source_files(apps_root):
        text = source_path.read_text(encoding="utf-8", errors="ignore")
        literals = extract_variable_literals(text)
        calls = extract_calls(text)

        for api in ("publisher", "subscription"):
            for type_name, topic_expr in calls[api]:
                topic_literal = resolve_string_expr(topic_expr, literals)
                if not topic_literal:
                    warnings.append(
                        f"{source_path}: unresolved {api} topic expression '{normalize_expr(topic_expr)}'"
                    )
                    continue
                ros_topic = normalize_ros_topic(topic_literal)
                msg_set.add(MsgUsage(type_name=type_name, ros_topic=ros_topic))

        for api in ("service", "client"):
            for type_name, service_expr in calls[api]:
                service_literal = resolve_string_expr(service_expr, literals)
                if not service_literal:
                    warnings.append(
                        f"{source_path}: unresolved {api} name expression '{normalize_expr(service_expr)}'"
                    )
                    continue
                ros_service = normalize_service_name(service_literal)
                srv_set.add(SrvUsage(type_name=type_name, ros_service=ros_service))

    msg_list = sorted(msg_set, key=lambda x: (x.ros_topic, x.type_name))
    srv_list = sorted(srv_set, key=lambda x: (x.ros_service, x.type_name))
    return msg_list, srv_list, warnings


def build_outputs(
    msg_usages: Iterable[MsgUsage],
    srv_usages: Iterable[SrvUsage],
    event_binding: str,
) -> Tuple[dict, dict]:
    used_service_ids: Set[int] = set()
    used_topic_event_ids: Set[int] = set()
    used_service_event_ids: Set[int] = set()
    used_method_ids: Set[int] = set()

    topic_mappings: List[dict] = []
    service_mappings: List[dict] = []

    provided_instances: List[dict] = []
    required_instances: List[dict] = []
    dds_topics: List[dict] = []

    # msg topics
    for index, usage in enumerate(msg_usages, start=1):
        interface_key = f"msg:{usage.type_name}:{usage.ros_topic}"
        service_id = allocate_u16(0x3000, 0x1000, interface_key, used_service_ids)
        event_id = allocate_u16(0x8000, 0x1000, interface_key, used_topic_event_ids)
        event_group_id = 0x0001

        interface_name = "Ros2Msg_" + to_identifier(f"{usage.type_name}_{usage.ros_topic}")
        instance_specifier = "/autosar/ros2/topic/" + usage.ros_topic.lstrip("/").replace("/", "_")
        dds_topic = ros_topic_to_dds_topic(usage.ros_topic)

        topic_mappings.append(
            {
                "ros_topic": usage.ros_topic,
                "dds_topic": dds_topic,
                "type_name": usage.type_name,
                "ara": {
                    "service_interface": interface_name,
                    "instance_specifier": instance_specifier,
                    "event": "data",
                    "event_binding": event_binding,
                    "service_interface_id": format_hex(service_id),
                    "service_instance_id": format_hex(0x0001),
                    "event_group_id": format_hex(event_group_id),
                    "event_id": format_hex(event_id),
                    "major_version": 1,
                    "minor_version": 0,
                },
            }
        )

        event_group_short = "DataEventGroup"
        provider_short = interface_name + "Provider"
        requirement_short = interface_name + "Requirement"

        provided_instances.append(
            {
                "short_name": provider_short,
                "service_interface": {
                    "id": format_hex(service_id),
                    "major": 1,
                    "minor": 0,
                },
                "service_instance_id": format_hex(0x0001),
                "event_groups": [
                    {
                        "short_name": event_group_short,
                        "event_group_id": format_hex(event_group_id),
                        "event_id": format_hex(event_id),
                        "multicast_udp_port": 30610 + index,
                        "ipv4_multicast_ip_address": "${SOMEIP_MCAST}",
                    }
                ],
                "sd_server": {
                    "initial_delay_min": 20,
                    "initial_delay_max": 200,
                },
            }
        )

        required_instances.append(
            {
                "short_name": requirement_short,
                "service_interface_id": format_hex(service_id),
                "major_version": 1,
                "minor_version": 0,
                "required_event_groups": [
                    {
                        "short_name": event_group_short + "Requirement",
                        "event_group_id": format_hex(event_group_id),
                        "event_id": format_hex(event_id),
                    }
                ],
                "sd_client": {
                    "initial_delay_min": 20,
                    "initial_delay_max": 200,
                },
            }
        )

        dds_topics.append(
            {
                "short_name": f"TopicTx{index:03d}",
                "topic_name": dds_topic,
                "type_name": usage.type_name,
                "qos_profile": "reliable_keep_last_10",
            }
        )

    # services (create_service/create_client)
    for index, usage in enumerate(srv_usages, start=1):
        interface_key = f"srv:{usage.type_name}:{usage.ros_service}"
        service_id = allocate_u16(0x5000, 0x1000, interface_key, used_service_ids)
        method_id = allocate_u16(0x0001, 0x7FFF, interface_key, used_method_ids)
        req_event_id = allocate_u16(0x8100, 0x0700, interface_key + ":req", used_service_event_ids)
        res_event_id = allocate_u16(0x8800, 0x0700, interface_key + ":res", used_service_event_ids)
        req_group_id = 0x0001
        res_group_id = 0x0002

        interface_name = "Ros2Srv_" + to_identifier(f"{usage.type_name}_{usage.ros_service}")
        instance_specifier = "/autosar/ros2/service/" + usage.ros_service.lstrip("/").replace("/", "_")
        req_ros, req_dds = service_to_request_topic(usage.ros_service)
        res_ros, res_dds = service_to_response_topic(usage.ros_service)

        request_type = f"{usage.type_name}_Request"
        response_type = f"{usage.type_name}_Response"

        topic_mappings.append(
            {
                "ros_topic": req_ros,
                "dds_topic": req_dds,
                "type_name": request_type,
                "ara": {
                    "service_interface": interface_name,
                    "instance_specifier": instance_specifier,
                    "event": "request",
                    "event_binding": event_binding,
                    "service_interface_id": format_hex(service_id),
                    "service_instance_id": format_hex(0x0001),
                    "event_group_id": format_hex(req_group_id),
                    "event_id": format_hex(req_event_id),
                    "major_version": 1,
                    "minor_version": 0,
                },
            }
        )

        topic_mappings.append(
            {
                "ros_topic": res_ros,
                "dds_topic": res_dds,
                "type_name": response_type,
                "ara": {
                    "service_interface": interface_name,
                    "instance_specifier": instance_specifier,
                    "event": "response",
                    "event_binding": event_binding,
                    "service_interface_id": format_hex(service_id),
                    "service_instance_id": format_hex(0x0001),
                    "event_group_id": format_hex(res_group_id),
                    "event_id": format_hex(res_event_id),
                    "major_version": 1,
                    "minor_version": 0,
                },
            }
        )

        service_mappings.append(
            {
                "ros_service": usage.ros_service,
                "request_topic": req_dds,
                "response_topic": res_dds,
                "request_type_name": request_type,
                "response_type_name": response_type,
                "ara": {
                    "service_interface": interface_name,
                    "instance_specifier": instance_specifier,
                    "method": "call",
                    "event_binding": event_binding,
                    "service_interface_id": format_hex(service_id),
                    "service_instance_id": format_hex(0x0001),
                    "method_id": format_hex(method_id),
                    "major_version": 1,
                    "minor_version": 0,
                },
            }
        )

        provided_instances.append(
            {
                "short_name": interface_name + "Provider",
                "service_interface": {
                    "id": format_hex(service_id),
                    "major": 1,
                    "minor": 0,
                },
                "service_instance_id": format_hex(0x0001),
                "event_groups": [
                    {
                        "short_name": "RequestEventGroup",
                        "event_group_id": format_hex(req_group_id),
                        "event_id": format_hex(req_event_id),
                        "multicast_udp_port": 30700 + (index * 2),
                        "ipv4_multicast_ip_address": "${SOMEIP_MCAST}",
                    },
                    {
                        "short_name": "ResponseEventGroup",
                        "event_group_id": format_hex(res_group_id),
                        "event_id": format_hex(res_event_id),
                        "multicast_udp_port": 30701 + (index * 2),
                        "ipv4_multicast_ip_address": "${SOMEIP_MCAST}",
                    },
                ],
                "sd_server": {
                    "initial_delay_min": 20,
                    "initial_delay_max": 200,
                },
            }
        )

        required_instances.append(
            {
                "short_name": interface_name + "Requirement",
                "service_interface_id": format_hex(service_id),
                "major_version": 1,
                "minor_version": 0,
                "required_event_groups": [
                    {
                        "short_name": "RequestEventGroupRequirement",
                        "event_group_id": format_hex(req_group_id),
                        "event_id": format_hex(req_event_id),
                    },
                    {
                        "short_name": "ResponseEventGroupRequirement",
                        "event_group_id": format_hex(res_group_id),
                        "event_id": format_hex(res_event_id),
                    },
                ],
                "sd_client": {
                    "initial_delay_min": 20,
                    "initial_delay_max": 200,
                },
            }
        )

        dds_topics.append(
            {
                "short_name": f"ServiceReqTx{index:03d}",
                "topic_name": req_dds,
                "type_name": request_type,
                "qos_profile": "reliable_keep_last_10",
            }
        )
        dds_topics.append(
            {
                "short_name": f"ServiceResTx{index:03d}",
                "topic_name": res_dds,
                "type_name": response_type,
                "qos_profile": "reliable_keep_last_10",
            }
        )

    mapping_output = {
        "version": 1,
        "topic_mappings": topic_mappings,
        "service_mappings": service_mappings,
    }

    manifest_output = {
        "variables": {
            "LOCAL_IP": "127.0.0.1",
            "SOMEIP_MCAST": "239.255.0.1",
        },
        "autosar": {
            "schema_namespace": "http://autosar.org/schema/r4.0",
            "schema_location": "http://autosar.org/schema/r4.0 autosar_00050.xsd",
            "packages": [
                {
                    "short_name": "AdaptiveAutosarManifest",
                    "runtime": {
                        "event_binding": event_binding,
                    },
                    "communication_cluster": {
                        "short_name": "AACommunicationCluster",
                        "protocol_version": 1,
                        "ethernet_physical_channels": [
                            {
                                "short_name": "LoopbackChannel",
                                "network_endpoints": [
                                    {
                                        "short_name": "AAProviderEndpoint",
                                        "ipv4_address": "${LOCAL_IP}",
                                    },
                                    {
                                        "short_name": "AAConsumerEndpoint",
                                        "ipv4_address": "${LOCAL_IP}",
                                    },
                                ],
                            }
                        ],
                    },
                    "ethernet_communication_connectors": [
                        {
                            "short_name": "AAProviderConnector",
                            "ap_application_endpoints": [
                                {
                                    "short_name": "AAProviderUdp",
                                    "transport": "udp",
                                    "port": 30601,
                                }
                            ],
                        },
                        {
                            "short_name": "AAConsumerConnector",
                            "ap_application_endpoints": [
                                {
                                    "short_name": "AAConsumerUdp",
                                    "transport": "udp",
                                    "port": 30602,
                                }
                            ],
                        },
                    ],
                    "someip": {
                        "provided_service_instances": provided_instances,
                        "required_service_instances": required_instances,
                    },
                    "dds": {
                        "short_name": "AADdsBinding",
                        "domain_id": 0,
                        "provided_topics": dds_topics,
                        "required_topics": dds_topics,
                    },
                }
            ],
        },
    }

    return mapping_output, manifest_output


def write_json_as_yaml(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate Adaptive AUTOSAR topic mapping + manifest from app sources."
    )
    parser.add_argument(
        "--apps-root",
        required=True,
        help="Root directory containing app sources to scan.",
    )
    parser.add_argument(
        "--output-mapping",
        required=True,
        help="Output path for autosar_topic_mapping.yaml",
    )
    parser.add_argument(
        "--output-manifest",
        required=True,
        help="Output path for autosar_manifest.yaml",
    )
    parser.add_argument(
        "--event-binding",
        default="auto",
        choices=sorted(VALID_EVENT_BINDINGS),
        help="Generated event binding policy (auto|dds|vsomeip).",
    )
    parser.add_argument(
        "--print-summary",
        action="store_true",
        help="Print generation summary.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    apps_root = Path(args.apps_root).resolve()
    output_mapping = Path(args.output_mapping).resolve()
    output_manifest = Path(args.output_manifest).resolve()

    if not apps_root.exists():
        sys.stderr.write(f"[autosar-mapping-gen] apps root not found: {apps_root}\n")
        return 1

    msg_usages, srv_usages, warnings = discover_usage(apps_root)
    event_binding = normalize_event_binding(args.event_binding)
    mapping_payload, manifest_payload = build_outputs(
        msg_usages,
        srv_usages,
        event_binding,
    )

    write_json_as_yaml(output_mapping, mapping_payload)
    write_json_as_yaml(output_manifest, manifest_payload)

    if args.print_summary:
        print("[autosar-mapping-gen] generation summary")
        print(f"  apps root: {apps_root}")
        print(f"  msg topics: {len(msg_usages)}")
        print(f"  services: {len(srv_usages)}")
        print(f"  event binding: {event_binding}")
        print(f"  mapping output: {output_mapping}")
        print(f"  manifest output: {output_manifest}")
        print(f"  unresolved expressions: {len(warnings)}")
        preview_count = min(20, len(warnings))
        for line in warnings[:preview_count]:
            print(f"  warning: {line}")
        if len(warnings) > preview_count:
            print(f"  warning: ... ({len(warnings) - preview_count} more)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
