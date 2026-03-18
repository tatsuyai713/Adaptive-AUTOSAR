#!/usr/bin/env python3
"""
YAML-driven AUTOSAR ARXML generator.

The generator focuses on communication-related manifest elements used in this
repository (SOME/IP service instances, endpoints, connectors) and also supports
custom raw elements for extensibility.
"""

from __future__ import annotations

import argparse
import ipaddress
import os
import string
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Tuple
import xml.etree.ElementTree as ET
from xml.dom import minidom

import yaml


DEFAULT_SCHEMA_NAMESPACE = "http://autosar.org/schema/r4.0"
DEFAULT_SCHEMA_LOCATION = "http://autosar.org/schema/r4.0 autosar_00050.xsd"
XSI_NAMESPACE = "http://www.w3.org/2001/XMLSchema-instance"


class ConfigError(ValueError):
    """Raised when input YAML cannot be converted into a valid ARXML tree."""


@dataclass
class GenerationSummary:
    package_count: int = 0
    communication_cluster_count: int = 0
    connector_count: int = 0
    provided_someip_count: int = 0
    required_someip_count: int = 0
    dds_binding_count: int = 0
    zerocopy_binding_count: int = 0
    custom_element_count: int = 0
    service_interface_count: int = 0
    execution_manifest_count: int = 0


@dataclass
class GenerationContext:
    strict: bool = False
    allow_extensions: bool = False
    warnings: List[str] = None
    summary: GenerationSummary = None

    def __post_init__(self) -> None:
        if self.warnings is None:
            self.warnings = []
        if self.summary is None:
            self.summary = GenerationSummary()

    def warn_or_fail(self, message: str) -> None:
        if self.strict:
            raise ConfigError(message)
        self.warnings.append(message)


def _listify(value: Any, context: str) -> List[Any]:
    if value is None:
        return []
    if isinstance(value, list):
        return value
    if isinstance(value, dict):
        return [value]
    raise ConfigError(f"{context}: expected list or mapping, got {type(value).__name__}")


def _as_mapping(value: Any, context: str, required: bool = True) -> Dict[str, Any]:
    if value is None:
        if required:
            raise ConfigError(f"{context}: missing mapping")
        return {}
    if not isinstance(value, dict):
        raise ConfigError(f"{context}: expected mapping, got {type(value).__name__}")
    return dict(value)


def _as_str(value: Any, context: str, required: bool = True) -> str:
    if value is None:
        if required:
            raise ConfigError(f"{context}: value is required")
        return ""
    if isinstance(value, (int, float, bool)):
        return str(value)
    if not isinstance(value, str):
        raise ConfigError(f"{context}: expected string, got {type(value).__name__}")
    text = value.strip()
    if required and not text:
        raise ConfigError(f"{context}: value must not be empty")
    return text


def _parse_int(
    value: Any,
    context: str,
    minimum: Optional[int] = None,
    maximum: Optional[int] = None,
) -> int:
    if isinstance(value, bool):
        raise ConfigError(f"{context}: boolean is not a valid integer")

    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str):
        text = value.strip().replace("_", "")
        if not text:
            raise ConfigError(f"{context}: empty integer string")
        base = 10
        if text.lower().startswith("0x"):
            base = 16
        elif text.lower().startswith("0b"):
            base = 2
        parsed = int(text, base)
    else:
        raise ConfigError(f"{context}: expected integer-like value, got {type(value).__name__}")

    if minimum is not None and parsed < minimum:
        raise ConfigError(f"{context}: value {parsed} is smaller than minimum {minimum}")
    if maximum is not None and parsed > maximum:
        raise ConfigError(f"{context}: value {parsed} is larger than maximum {maximum}")

    return parsed


def _parse_bool(value: Any, context: str, default: bool = False) -> bool:
    if value is None:
        return default

    if isinstance(value, bool):
        return value

    if isinstance(value, int):
        return value != 0

    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False

    raise ConfigError(
        f"{context}: expected boolean-like value "
        "(true/false/1/0/yes/no/on/off)")


def _validate_ipv4(value: Any, context: str) -> str:
    text = _as_str(value, context)
    try:
        ip = ipaddress.ip_address(text)
    except ValueError as exc:
        raise ConfigError(f"{context}: invalid IP address '{text}'") from exc
    if ip.version != 4:
        raise ConfigError(f"{context}: expected IPv4 address, got '{text}'")
    return str(ip)


def _warn_unknown_keys(
    mapping: Mapping[str, Any],
    known_keys: Iterable[str],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    known = set(known_keys)
    unknown = sorted([k for k in mapping.keys() if k not in known])
    for key in unknown:
        gen_ctx.warn_or_fail(f"{context}: unknown key '{key}'")


def _substitute_variables(value: Any, variables: Mapping[str, str]) -> Any:
    if isinstance(value, dict):
        return {k: _substitute_variables(v, variables) for k, v in value.items()}
    if isinstance(value, list):
        return [_substitute_variables(v, variables) for v in value]
    if isinstance(value, str):
        expanded = os.path.expandvars(value)
        return string.Template(expanded).safe_substitute(variables)
    return value


def _load_yaml(path: Path) -> Dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as f:
            loaded = yaml.safe_load(f)
    except FileNotFoundError as exc:
        raise ConfigError(f"Input file not found: {path}") from exc
    except yaml.YAMLError as exc:
        raise ConfigError(f"Invalid YAML in file '{path}': {exc}") from exc

    if loaded is None:
        raise ConfigError(f"Input file '{path}' is empty")
    if not isinstance(loaded, dict):
        raise ConfigError(f"Root of YAML file '{path}' must be a mapping")
    return loaded


def _extract_autosar_section(doc: Dict[str, Any], context: str) -> Dict[str, Any]:
    autosar = doc.get("autosar", doc)
    autosar_map = _as_mapping(autosar, f"{context}.autosar")
    return autosar_map


def _merge_documents(documents: List[Dict[str, Any]]) -> Dict[str, Any]:
    merged_variables: Dict[str, str] = {}
    for doc in documents:
        variables = _as_mapping(doc.get("variables", {}), "variables", required=False)
        for key, value in variables.items():
            merged_variables[str(key)] = str(value)

    substituted = [_substitute_variables(doc, merged_variables) for doc in documents]

    merged_autosar: Dict[str, Any] = {
        "schema_namespace": DEFAULT_SCHEMA_NAMESPACE,
        "schema_location": DEFAULT_SCHEMA_LOCATION,
        "packages": [],
    }

    for idx, doc in enumerate(substituted):
        ctx = f"document[{idx}]"
        autosar = _extract_autosar_section(doc, ctx)

        namespace = autosar.get("schema_namespace")
        if namespace:
            merged_autosar["schema_namespace"] = _as_str(namespace, f"{ctx}.schema_namespace")

        schema_location = autosar.get("schema_location")
        if schema_location:
            merged_autosar["schema_location"] = _as_str(schema_location, f"{ctx}.schema_location")

        package_list = []
        package_list.extend(_listify(autosar.get("packages"), f"{ctx}.packages"))
        package_list.extend(_listify(autosar.get("package"), f"{ctx}.package"))

        if not package_list:
            raise ConfigError(f"{ctx}: no packages found; use 'autosar.packages' or 'autosar.package'")

        merged_autosar["packages"].extend(package_list)

    return {"autosar": merged_autosar}


def _add_text(parent: ET.Element, tag: str, text: Any) -> ET.Element:
    element = ET.SubElement(parent, tag)
    element.text = str(text)
    return element


def _service_interface_values(instance: Dict[str, Any], context: str) -> Tuple[int, int, int]:
    service_interface = _as_mapping(instance.get("service_interface", {}), f"{context}.service_interface", required=False)

    service_id = instance.get("service_interface_id", service_interface.get("id"))
    if service_id is None:
        raise ConfigError(f"{context}: service_interface_id is required")

    major_version = instance.get("major_version", service_interface.get("major", 1))
    minor_version = instance.get("minor_version", service_interface.get("minor", 0))

    service_id_int = _parse_int(service_id, f"{context}.service_interface_id", 0, 0xFFFF)
    major_int = _parse_int(major_version, f"{context}.major_version", 0, 0xFF)
    minor_int = _parse_int(minor_version, f"{context}.minor_version", 0, 0xFFFFFFFF)

    return service_id_int, major_int, minor_int


def _build_communication_cluster(
    elements_parent: ET.Element,
    cluster: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    cluster_map = _as_mapping(cluster, context)
    _warn_unknown_keys(
        cluster_map,
        {
            "short_name",
            "protocol_version",
            "ethernet_physical_channels",
            "ethernet_physical_channel",
        },
        context,
        gen_ctx,
    )

    cluster_element = ET.SubElement(elements_parent, "COMMUNICATION-CLUSTER")
    short_name = _as_str(cluster_map.get("short_name", "CommunicationCluster"), f"{context}.short_name")
    protocol_version = _parse_int(cluster_map.get("protocol_version", 1), f"{context}.protocol_version", 0)

    channels = []
    channels.extend(_listify(cluster_map.get("ethernet_physical_channels"), f"{context}.ethernet_physical_channels"))
    channels.extend(_listify(cluster_map.get("ethernet_physical_channel"), f"{context}.ethernet_physical_channel"))

    if not channels:
        raise ConfigError(f"{context}: at least one ethernet physical channel is required")

    for channel_index, channel in enumerate(channels):
        channel_ctx = f"{context}.ethernet_physical_channels[{channel_index}]"
        channel_map = _as_mapping(channel, channel_ctx)
        _warn_unknown_keys(channel_map, {"short_name", "network_endpoints"}, channel_ctx, gen_ctx)

        physical = ET.SubElement(cluster_element, "ETHERNET-PHYSICAL-CHANNEL")
        ch_name_default = f"{short_name}Channel{channel_index + 1}"
        _add_text(physical, "SHORT-NAME", _as_str(channel_map.get("short_name", ch_name_default), f"{channel_ctx}.short_name"))

        endpoints = _listify(channel_map.get("network_endpoints"), f"{channel_ctx}.network_endpoints")
        if not endpoints:
            raise ConfigError(f"{channel_ctx}: at least one network endpoint is required")

        ne_parent = ET.SubElement(physical, "NETWORK-ENDPOINTS")
        for endpoint_index, endpoint in enumerate(endpoints):
            endpoint_ctx = f"{channel_ctx}.network_endpoints[{endpoint_index}]"
            endpoint_map = _as_mapping(endpoint, endpoint_ctx)
            _warn_unknown_keys(endpoint_map, {"short_name", "ipv4_address"}, endpoint_ctx, gen_ctx)

            ne = ET.SubElement(ne_parent, "NETWORK-ENDPOINT")
            default_name = f"Endpoint{endpoint_index + 1}"
            _add_text(ne, "SHORT-NAME", _as_str(endpoint_map.get("short_name", default_name), f"{endpoint_ctx}.short_name"))

            addrs = ET.SubElement(ne, "NETWORK-ENDPOINT-ADDRESSES")
            ipv4_cfg = ET.SubElement(addrs, "IPV-4-CONFIGURATION")
            ipv4 = _validate_ipv4(endpoint_map.get("ipv4_address", "127.0.0.1"), f"{endpoint_ctx}.ipv4_address")
            _add_text(ipv4_cfg, "IPV-4-ADDRESS", ipv4)

    _add_text(cluster_element, "PROTOCOL-VERSION", protocol_version)
    gen_ctx.summary.communication_cluster_count += 1


def _build_connector(
    elements_parent: ET.Element,
    connector: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    connector_map = _as_mapping(connector, context)
    _warn_unknown_keys(
        connector_map,
        {"short_name", "ap_application_endpoints", "ap_application_endpoint"},
        context,
        gen_ctx,
    )

    conn = ET.SubElement(elements_parent, "ETHERNET-COMMUNICATION-CONNECTOR")
    _add_text(conn, "SHORT-NAME", _as_str(connector_map.get("short_name"), f"{context}.short_name"))

    ap_eps = []
    ap_eps.extend(_listify(connector_map.get("ap_application_endpoints"), f"{context}.ap_application_endpoints"))
    ap_eps.extend(_listify(connector_map.get("ap_application_endpoint"), f"{context}.ap_application_endpoint"))

    if not ap_eps:
        raise ConfigError(f"{context}: at least one ap_application_endpoint is required")

    ap_parent = ET.SubElement(conn, "AP-APPLICATION-ENDPOINTS")
    for endpoint_index, app_endpoint in enumerate(ap_eps):
        endpoint_ctx = f"{context}.ap_application_endpoints[{endpoint_index}]"
        endpoint_map = _as_mapping(app_endpoint, endpoint_ctx)
        _warn_unknown_keys(endpoint_map, {"short_name", "transport", "port"}, endpoint_ctx, gen_ctx)

        ap = ET.SubElement(ap_parent, "AP-APPLICATION-ENDPOINT")
        _add_text(ap, "SHORT-NAME", _as_str(endpoint_map.get("short_name"), f"{endpoint_ctx}.short_name"))

        transport = _as_str(endpoint_map.get("transport"), f"{endpoint_ctx}.transport").lower()
        port = _parse_int(endpoint_map.get("port"), f"{endpoint_ctx}.port", 1, 65535)

        tp_cfg = ET.SubElement(ap, "TP-CONFIGURATION")
        if transport == "udp":
            udp = ET.SubElement(tp_cfg, "UDP-TP")
            udp_port = ET.SubElement(udp, "UDP-TP-PORT")
            _add_text(udp_port, "PORT-NUMBER", port)
        elif transport == "tcp":
            tcp = ET.SubElement(tp_cfg, "TCP-TP")
            tcp_port = ET.SubElement(tcp, "TCP-TP-PORT")
            _add_text(tcp_port, "PORT-NUMBER", port)
        else:
            raise ConfigError(f"{endpoint_ctx}.transport: expected 'udp' or 'tcp', got '{transport}'")

    gen_ctx.summary.connector_count += 1


def _build_service_interface(
    elements_parent: ET.Element,
    si: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    """Build a SERVICE-INTERFACE element (events, methods, fields)."""
    si_map = _as_mapping(si, context)
    _warn_unknown_keys(
        si_map,
        {"short_name", "events", "methods", "fields"},
        context,
        gen_ctx,
    )

    elem = ET.SubElement(elements_parent, "SERVICE-INTERFACE")
    _add_text(elem, "SHORT-NAME", _as_str(si_map.get("short_name"), f"{context}.short_name"))

    events = _listify(si_map.get("events"), f"{context}.events")
    if events:
        events_parent = ET.SubElement(elem, "EVENTS")
        for idx, event in enumerate(events):
            event_ctx = f"{context}.events[{idx}]"
            event_map = _as_mapping(event, event_ctx)
            _warn_unknown_keys(event_map, {"short_name", "type_ref"}, event_ctx, gen_ctx)
            proto = ET.SubElement(events_parent, "VARIABLE-DATA-PROTOTYPE")
            _add_text(proto, "SHORT-NAME", _as_str(event_map.get("short_name"), f"{event_ctx}.short_name"))
            type_ref = event_map.get("type_ref")
            if type_ref is not None:
                tr = ET.SubElement(proto, "TYPE-TREF")
                tr.set("DEST", "IMPLEMENTATION-DATA-TYPE")
                tr.text = _as_str(type_ref, f"{event_ctx}.type_ref")

    methods = _listify(si_map.get("methods"), f"{context}.methods")
    if methods:
        methods_parent = ET.SubElement(elem, "METHODS")
        for idx, method in enumerate(methods):
            method_ctx = f"{context}.methods[{idx}]"
            method_map = _as_mapping(method, method_ctx)
            _warn_unknown_keys(
                method_map,
                {"short_name", "arguments", "fire_and_forget"},
                method_ctx,
                gen_ctx,
            )
            op = ET.SubElement(methods_parent, "CLIENT-SERVER-OPERATION")
            _add_text(op, "SHORT-NAME", _as_str(method_map.get("short_name"), f"{method_ctx}.short_name"))
            if _parse_bool(method_map.get("fire_and_forget"), f"{method_ctx}.fire_and_forget"):
                _add_text(op, "FIRE-AND-FORGET", "true")
            arguments = _listify(method_map.get("arguments"), f"{method_ctx}.arguments")
            if arguments:
                args_parent = ET.SubElement(op, "ARGUMENTS")
                for aidx, arg in enumerate(arguments):
                    arg_ctx = f"{method_ctx}.arguments[{aidx}]"
                    arg_map = _as_mapping(arg, arg_ctx)
                    _warn_unknown_keys(arg_map, {"short_name", "direction", "type_ref"}, arg_ctx, gen_ctx)
                    arg_elem = ET.SubElement(args_parent, "ARGUMENT-DATA-PROTOTYPE")
                    _add_text(arg_elem, "SHORT-NAME", _as_str(arg_map.get("short_name"), f"{arg_ctx}.short_name"))
                    direction = _as_str(arg_map.get("direction", "in"), f"{arg_ctx}.direction").upper()
                    _add_text(arg_elem, "DIRECTION", direction)
                    type_ref = arg_map.get("type_ref")
                    if type_ref is not None:
                        tr = ET.SubElement(arg_elem, "TYPE-TREF")
                        tr.set("DEST", "IMPLEMENTATION-DATA-TYPE")
                        tr.text = _as_str(type_ref, f"{arg_ctx}.type_ref")

    fields = _listify(si_map.get("fields"), f"{context}.fields")
    if fields:
        fields_parent = ET.SubElement(elem, "FIELDS")
        for idx, field in enumerate(fields):
            field_ctx = f"{context}.fields[{idx}]"
            field_map = _as_mapping(field, field_ctx)
            _warn_unknown_keys(
                field_map,
                {"short_name", "has_getter", "has_setter", "has_notifier", "type_ref"},
                field_ctx,
                gen_ctx,
            )
            field_elem = ET.SubElement(fields_parent, "FIELD")
            _add_text(field_elem, "SHORT-NAME", _as_str(field_map.get("short_name"), f"{field_ctx}.short_name"))
            _add_text(field_elem, "HAS-GETTER", str(_parse_bool(field_map.get("has_getter", True), f"{field_ctx}.has_getter")).lower())
            _add_text(field_elem, "HAS-SETTER", str(_parse_bool(field_map.get("has_setter", False), f"{field_ctx}.has_setter")).lower())
            _add_text(field_elem, "HAS-NOTIFIER", str(_parse_bool(field_map.get("has_notifier", False), f"{field_ctx}.has_notifier")).lower())
            type_ref = field_map.get("type_ref")
            if type_ref is not None:
                tr = ET.SubElement(field_elem, "TYPE-TREF")
                tr.set("DEST", "IMPLEMENTATION-DATA-TYPE")
                tr.text = _as_str(type_ref, f"{field_ctx}.type_ref")

    gen_ctx.summary.service_interface_count += 1


def _build_execution_manifest(
    elements_parent: ET.Element,
    em: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    """Build an EXECUTION-MANIFEST element (process-to-machine mappings, startup configs)."""
    em_map = _as_mapping(em, context)
    _warn_unknown_keys(
        em_map,
        {"short_name", "process_to_machine_mappings", "startup_configs"},
        context,
        gen_ctx,
    )

    elem = ET.SubElement(elements_parent, "EXECUTION-MANIFEST")
    _add_text(elem, "SHORT-NAME", _as_str(em_map.get("short_name"), f"{context}.short_name"))

    mappings = _listify(em_map.get("process_to_machine_mappings"), f"{context}.process_to_machine_mappings")
    if mappings:
        mappings_parent = ET.SubElement(elem, "PROCESS-TO-MACHINE-MAPPINGS")
        for idx, mapping in enumerate(mappings):
            mapping_ctx = f"{context}.process_to_machine_mappings[{idx}]"
            mapping_map = _as_mapping(mapping, mapping_ctx)
            _warn_unknown_keys(
                mapping_map,
                {"short_name", "process_ref", "machine_ref"},
                mapping_ctx,
                gen_ctx,
            )
            m_elem = ET.SubElement(mappings_parent, "PROCESS-TO-MACHINE-MAPPING")
            _add_text(
                m_elem,
                "SHORT-NAME",
                _as_str(mapping_map.get("short_name", f"Mapping{idx + 1}"), f"{mapping_ctx}.short_name"),
            )
            process_ref = mapping_map.get("process_ref")
            if process_ref is not None:
                pr = ET.SubElement(m_elem, "PROCESS-REF")
                pr.set("DEST", "PROCESS")
                pr.text = _as_str(process_ref, f"{mapping_ctx}.process_ref")
            machine_ref = mapping_map.get("machine_ref")
            if machine_ref is not None:
                mr = ET.SubElement(m_elem, "MACHINE-REF")
                mr.set("DEST", "MACHINE")
                mr.text = _as_str(machine_ref, f"{mapping_ctx}.machine_ref")

    startup_configs = _listify(em_map.get("startup_configs"), f"{context}.startup_configs")
    if startup_configs:
        configs_parent = ET.SubElement(elem, "STARTUP-CONFIGS")
        for idx, config in enumerate(startup_configs):
            config_ctx = f"{context}.startup_configs[{idx}]"
            config_map = _as_mapping(config, config_ctx)
            _warn_unknown_keys(config_map, {"short_name", "arguments"}, config_ctx, gen_ctx)
            cfg_elem = ET.SubElement(configs_parent, "STARTUP-CONFIG")
            _add_text(
                cfg_elem,
                "SHORT-NAME",
                _as_str(config_map.get("short_name", f"StartupConfig{idx + 1}"), f"{config_ctx}.short_name"),
            )
            arguments = _listify(config_map.get("arguments"), f"{config_ctx}.arguments")
            if arguments:
                args_parent = ET.SubElement(cfg_elem, "ARGUMENTS")
                for aidx, arg in enumerate(arguments):
                    arg_ctx = f"{config_ctx}.arguments[{aidx}]"
                    arg_map = _as_mapping(arg, arg_ctx)
                    _warn_unknown_keys(arg_map, {"short_name", "value"}, arg_ctx, gen_ctx)
                    arg_elem = ET.SubElement(args_parent, "STARTUP-ARGUMENT")
                    _add_text(
                        arg_elem,
                        "SHORT-NAME",
                        _as_str(arg_map.get("short_name", f"Arg{aidx + 1}"), f"{arg_ctx}.short_name"),
                    )
                    _add_text(
                        arg_elem,
                        "VALUE",
                        _as_str(arg_map.get("value", ""), f"{arg_ctx}.value", required=False),
                    )

    gen_ctx.summary.execution_manifest_count += 1


def _build_provided_someip_instance(
    elements_parent: ET.Element,
    instance: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    instance_map = _as_mapping(instance, context)
    _warn_unknown_keys(
        instance_map,
        {
            "short_name",
            "service_interface",
            "service_interface_id",
            "major_version",
            "minor_version",
            "service_instance_id",
            "event_groups",
            "methods",
            "sd_server",
        },
        context,
        gen_ctx,
    )

    elem = ET.SubElement(elements_parent, "PROVIDED-SOMEIP-SERVICE-INSTANCE")
    _add_text(elem, "SHORT-NAME", _as_str(instance_map.get("short_name"), f"{context}.short_name"))

    service_id, major, minor = _service_interface_values(instance_map, context)
    deployment = ET.SubElement(elem, "SERVICE-INTERFACE-DEPLOYMENT")
    _add_text(deployment, "SERVICE-INTERFACE-ID", service_id)

    version = ET.SubElement(deployment, "SERVICE-INTERFACE-VERSION")
    _add_text(version, "MAJOR-VERSION", major)
    _add_text(version, "MINOR-VERSION", minor)

    methods = _listify(instance_map.get("methods"), f"{context}.methods")
    if methods:
        method_deployments = ET.SubElement(deployment, "METHOD-DEPLOYMENTS")
        for idx, method in enumerate(methods):
            method_ctx = f"{context}.methods[{idx}]"
            method_map = _as_mapping(method, method_ctx)
            _warn_unknown_keys(method_map, {"short_name", "method_id"}, method_ctx, gen_ctx)
            md_elem = ET.SubElement(method_deployments, "SOMEIP-METHOD-DEPLOYMENT")
            _add_text(md_elem, "SHORT-NAME", _as_str(method_map.get("short_name"), f"{method_ctx}.short_name"))
            _add_text(
                md_elem,
                "METHOD-ID",
                _parse_int(method_map.get("method_id"), f"{method_ctx}.method_id", 0, 0xFFFF),
            )

    event_groups = _listify(instance_map.get("event_groups"), f"{context}.event_groups")
    if event_groups:
        provided = ET.SubElement(elem, "PROVIDED-EVENT-GROUPS")
        for event_index, event_group in enumerate(event_groups):
            group_ctx = f"{context}.event_groups[{event_index}]"
            group_map = _as_mapping(event_group, group_ctx)
            _warn_unknown_keys(
                group_map,
                {
                    "short_name",
                    "event_group_id",
                    "event_id",
                    "multicast_udp_port",
                    "ipv4_multicast_ip_address",
                },
                group_ctx,
                gen_ctx,
            )

            group = ET.SubElement(provided, "SOMEIP-PROVIDED-EVENT-GROUP")
            _add_text(group, "SHORT-NAME", _as_str(group_map.get("short_name"), f"{group_ctx}.short_name"))
            _add_text(group, "EVENT-GROUP-ID", _parse_int(group_map.get("event_group_id"), f"{group_ctx}.event_group_id", 0, 0xFFFF))
            _add_text(group, "EVENT-ID", _parse_int(group_map.get("event_id"), f"{group_ctx}.event_id", 0, 0xFFFF))

            multicast_port = group_map.get("multicast_udp_port")
            if multicast_port is not None:
                _add_text(group, "EVENT-MULTICAST-UDP-PORT", _parse_int(multicast_port, f"{group_ctx}.multicast_udp_port", 1, 65535))

            multicast_ip = group_map.get("ipv4_multicast_ip_address")
            if multicast_ip is not None:
                _add_text(group, "IPV-4-MULTICAST-IP-ADDRESS", _validate_ipv4(multicast_ip, f"{group_ctx}.ipv4_multicast_ip_address"))

    sd_server = _as_mapping(instance_map.get("sd_server", {}), f"{context}.sd_server", required=False)
    if sd_server:
        _warn_unknown_keys(sd_server, {"initial_delay_min", "initial_delay_max"}, f"{context}.sd_server", gen_ctx)
        sd_cfg = ET.SubElement(elem, "SD-SERVER-CONFIG")
        behavior = ET.SubElement(sd_cfg, "INITIAL-OFFER-BEHAVIOR")
        _add_text(behavior, "INITIAL-DELAY-MIN-VALUE", _parse_int(sd_server.get("initial_delay_min", 20), f"{context}.sd_server.initial_delay_min", 0))
        _add_text(behavior, "INITIAL-DELAY-MAX-VALUE", _parse_int(sd_server.get("initial_delay_max", 200), f"{context}.sd_server.initial_delay_max", 0))

    _add_text(
        elem,
        "SERVICE-INSTANCE-ID",
        _parse_int(instance_map.get("service_instance_id"), f"{context}.service_instance_id", 0, 0xFFFF),
    )

    gen_ctx.summary.provided_someip_count += 1


def _build_required_someip_instance(
    elements_parent: ET.Element,
    instance: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    instance_map = _as_mapping(instance, context)
    _warn_unknown_keys(
        instance_map,
        {
            "short_name",
            "service_interface",
            "service_interface_id",
            "major_version",
            "minor_version",
            "required_event_groups",
            "event_groups",
            "sd_client",
        },
        context,
        gen_ctx,
    )

    elem = ET.SubElement(elements_parent, "REQUIRED-SOMEIP-SERVICE-INSTANCE")
    _add_text(elem, "SHORT-NAME", _as_str(instance_map.get("short_name"), f"{context}.short_name"))

    service_id, major, minor = _service_interface_values(instance_map, context)
    deployment = ET.SubElement(elem, "SERVICE-INTERFACE-DEPLOYMENT")
    _add_text(deployment, "SERVICE-INTERFACE-ID", service_id)

    version = ET.SubElement(deployment, "SERVICE-INTERFACE-VERSION")
    _add_text(version, "MAJOR-VERSION", major)
    _add_text(version, "MINOR-VERSION", minor)

    event_groups = []
    event_groups.extend(_listify(instance_map.get("required_event_groups"), f"{context}.required_event_groups"))
    event_groups.extend(_listify(instance_map.get("event_groups"), f"{context}.event_groups"))

    if event_groups:
        required = ET.SubElement(elem, "REQUIRED-EVENT-GROUPS")
        for event_index, event_group in enumerate(event_groups):
            group_ctx = f"{context}.event_groups[{event_index}]"
            group_map = _as_mapping(event_group, group_ctx)
            _warn_unknown_keys(group_map, {"short_name", "event_group_id", "event_id"}, group_ctx, gen_ctx)

            group = ET.SubElement(required, "SOMEIP-REQUIRED-EVENT-GROUP")
            _add_text(group, "SHORT-NAME", _as_str(group_map.get("short_name"), f"{group_ctx}.short_name"))
            _add_text(group, "EVENT-GROUP-ID", _parse_int(group_map.get("event_group_id"), f"{group_ctx}.event_group_id", 0, 0xFFFF))
            _add_text(group, "EVENT-ID", _parse_int(group_map.get("event_id"), f"{group_ctx}.event_id", 0, 0xFFFF))

    sd_client = _as_mapping(instance_map.get("sd_client", {}), f"{context}.sd_client", required=False)
    if sd_client:
        _warn_unknown_keys(sd_client, {"initial_delay_min", "initial_delay_max"}, f"{context}.sd_client", gen_ctx)
        sd_cfg = ET.SubElement(elem, "SD-CLIENT-CONFIG")
        behavior = ET.SubElement(sd_cfg, "INITIAL-FIND-BEHAVIOR")
        _add_text(behavior, "INITIAL-DELAY-MIN-VALUE", _parse_int(sd_client.get("initial_delay_min", 20), f"{context}.sd_client.initial_delay_min", 0))
        _add_text(behavior, "INITIAL-DELAY-MAX-VALUE", _parse_int(sd_client.get("initial_delay_max", 200), f"{context}.sd_client.initial_delay_max", 0))

    gen_ctx.summary.required_someip_count += 1


def _build_dds_binding(
    elements_parent: ET.Element,
    dds_config: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    dds_map = _as_mapping(dds_config, context, required=False)
    if not dds_map:
        return

    _warn_unknown_keys(
        dds_map,
        {
            "short_name",
            "domain_id",
            "provided_topics",
            "publish_topics",
            "required_topics",
            "subscribe_topics",
        },
        context,
        gen_ctx,
    )

    binding = ET.SubElement(elements_parent, "DDS-BINDING")
    _add_text(
        binding,
        "SHORT-NAME",
        _as_str(dds_map.get("short_name", "DdsBinding"), f"{context}.short_name"))
    _add_text(
        binding,
        "DOMAIN-ID",
        _parse_int(dds_map.get("domain_id", 0), f"{context}.domain_id", 0))

    provided_topics = []
    provided_topics.extend(_listify(dds_map.get("provided_topics"), f"{context}.provided_topics"))
    provided_topics.extend(_listify(dds_map.get("publish_topics"), f"{context}.publish_topics"))

    if provided_topics:
        provided_parent = ET.SubElement(binding, "PROVIDED-TOPICS")
        for idx, topic in enumerate(provided_topics):
            topic_ctx = f"{context}.provided_topics[{idx}]"
            topic_map = _as_mapping(topic, topic_ctx)
            _warn_unknown_keys(
                topic_map,
                {"short_name", "topic_name", "type_name", "qos_profile", "domain_id"},
                topic_ctx,
                gen_ctx,
            )

            topic_elem = ET.SubElement(provided_parent, "DDS-PROVIDED-TOPIC")
            _add_text(
                topic_elem,
                "SHORT-NAME",
                _as_str(topic_map.get("short_name", f"ProvidedTopic{idx + 1}"), f"{topic_ctx}.short_name"))
            _add_text(topic_elem, "TOPIC-NAME", _as_str(topic_map.get("topic_name"), f"{topic_ctx}.topic_name"))
            _add_text(topic_elem, "TYPE-NAME", _as_str(topic_map.get("type_name"), f"{topic_ctx}.type_name"))

            topic_domain_id = topic_map.get("domain_id")
            if topic_domain_id is not None:
                _add_text(topic_elem, "DOMAIN-ID", _parse_int(topic_domain_id, f"{topic_ctx}.domain_id", 0))

            qos_profile = topic_map.get("qos_profile")
            if qos_profile is not None:
                _add_text(topic_elem, "QOS-PROFILE", _as_str(qos_profile, f"{topic_ctx}.qos_profile"))

    required_topics = []
    required_topics.extend(_listify(dds_map.get("required_topics"), f"{context}.required_topics"))
    required_topics.extend(_listify(dds_map.get("subscribe_topics"), f"{context}.subscribe_topics"))

    if required_topics:
        required_parent = ET.SubElement(binding, "REQUIRED-TOPICS")
        for idx, topic in enumerate(required_topics):
            topic_ctx = f"{context}.required_topics[{idx}]"
            topic_map = _as_mapping(topic, topic_ctx)
            _warn_unknown_keys(
                topic_map,
                {"short_name", "topic_name", "type_name", "qos_profile", "domain_id"},
                topic_ctx,
                gen_ctx,
            )

            topic_elem = ET.SubElement(required_parent, "DDS-REQUIRED-TOPIC")
            _add_text(
                topic_elem,
                "SHORT-NAME",
                _as_str(topic_map.get("short_name", f"RequiredTopic{idx + 1}"), f"{topic_ctx}.short_name"))
            _add_text(topic_elem, "TOPIC-NAME", _as_str(topic_map.get("topic_name"), f"{topic_ctx}.topic_name"))
            _add_text(topic_elem, "TYPE-NAME", _as_str(topic_map.get("type_name"), f"{topic_ctx}.type_name"))

            topic_domain_id = topic_map.get("domain_id")
            if topic_domain_id is not None:
                _add_text(topic_elem, "DOMAIN-ID", _parse_int(topic_domain_id, f"{topic_ctx}.domain_id", 0))

            qos_profile = topic_map.get("qos_profile")
            if qos_profile is not None:
                _add_text(topic_elem, "QOS-PROFILE", _as_str(qos_profile, f"{topic_ctx}.qos_profile"))

    gen_ctx.summary.dds_binding_count += 1


def _build_zerocopy_binding(
    elements_parent: ET.Element,
    zerocopy_config: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> None:
    zerocopy_map = _as_mapping(zerocopy_config, context, required=False)
    if not zerocopy_map:
        return

    _warn_unknown_keys(
        zerocopy_map,
        {
            "short_name",
            "runtime",
            "enabled",
            "service_channels",
            "service_instances",
        },
        context,
        gen_ctx,
    )

    binding = ET.SubElement(elements_parent, "EXT-ZEROCOPY-BINDING")
    _add_text(
        binding,
        "SHORT-NAME",
        _as_str(
            zerocopy_map.get("short_name", "ZeroCopyBinding"),
            f"{context}.short_name"))
    _add_text(
        binding,
        "RUNTIME",
        _as_str(
            zerocopy_map.get("runtime", "iceoryx"),
            f"{context}.runtime"))
    _add_text(
        binding,
        "ENABLED",
        str(
            _parse_bool(
                zerocopy_map.get("enabled", True),
                f"{context}.enabled",
                default=True)).lower())

    channels = []
    channels.extend(_listify(zerocopy_map.get("service_channels"), f"{context}.service_channels"))
    channels.extend(_listify(zerocopy_map.get("service_instances"), f"{context}.service_instances"))

    if channels:
        channels_parent = ET.SubElement(binding, "SERVICE-CHANNELS")
        for idx, channel in enumerate(channels):
            channel_ctx = f"{context}.service_channels[{idx}]"
            channel_map = _as_mapping(channel, channel_ctx)
            _warn_unknown_keys(
                channel_map,
                {
                    "short_name",
                    "service_id",
                    "instance_id",
                    "event_id",
                    "publisher_history",
                    "subscriber_queue",
                    "max_sample_size",
                    "max_publishers",
                    "max_subscribers",
                },
                channel_ctx,
                gen_ctx,
            )

            channel_elem = ET.SubElement(
                channels_parent,
                "EXT-ZEROCOPY-SERVICE-CHANNEL")
            _add_text(
                channel_elem,
                "SHORT-NAME",
                _as_str(
                    channel_map.get("short_name", f"ZeroCopyChannel{idx + 1}"),
                    f"{channel_ctx}.short_name"))
            _add_text(
                channel_elem,
                "SERVICE-ID",
                _parse_int(channel_map.get("service_id"), f"{channel_ctx}.service_id", 0, 0xFFFF))
            _add_text(
                channel_elem,
                "INSTANCE-ID",
                _parse_int(channel_map.get("instance_id"), f"{channel_ctx}.instance_id", 0, 0xFFFF))
            _add_text(
                channel_elem,
                "EVENT-ID",
                _parse_int(channel_map.get("event_id"), f"{channel_ctx}.event_id", 0, 0xFFFF))
            _add_text(
                channel_elem,
                "PUBLISHER-HISTORY",
                _parse_int(channel_map.get("publisher_history", 16), f"{channel_ctx}.publisher_history", 1))
            _add_text(
                channel_elem,
                "SUBSCRIBER-QUEUE",
                _parse_int(channel_map.get("subscriber_queue", 16), f"{channel_ctx}.subscriber_queue", 1))
            _add_text(
                channel_elem,
                "MAX-SAMPLE-SIZE",
                _parse_int(channel_map.get("max_sample_size", 1024), f"{channel_ctx}.max_sample_size", 1))

            max_publishers = channel_map.get("max_publishers")
            if max_publishers is not None:
                _add_text(
                    channel_elem,
                    "MAX-PUBLISHERS",
                    _parse_int(max_publishers, f"{channel_ctx}.max_publishers", 1))

            max_subscribers = channel_map.get("max_subscribers")
            if max_subscribers is not None:
                _add_text(
                    channel_elem,
                    "MAX-SUBSCRIBERS",
                    _parse_int(max_subscribers, f"{channel_ctx}.max_subscribers", 1))

    gen_ctx.summary.zerocopy_binding_count += 1


def _build_custom_element(
    parent: ET.Element,
    spec: Dict[str, Any],
    context: str,
    gen_ctx: GenerationContext,
) -> ET.Element:
    spec_map = _as_mapping(spec, context)
    _warn_unknown_keys(spec_map, {"tag", "attributes", "text", "short_name", "children"}, context, gen_ctx)

    tag = _as_str(spec_map.get("tag"), f"{context}.tag")
    attributes = _as_mapping(spec_map.get("attributes", {}), f"{context}.attributes", required=False)
    attributes_text = {str(k): str(v) for k, v in attributes.items()}

    elem = ET.SubElement(parent, tag, attributes_text)

    short_name = spec_map.get("short_name")
    if short_name is not None:
        _add_text(elem, "SHORT-NAME", _as_str(short_name, f"{context}.short_name"))

    text = spec_map.get("text")
    if text is not None:
        elem.text = str(text)

    children = _listify(spec_map.get("children"), f"{context}.children")
    for idx, child in enumerate(children):
        _build_custom_element(elem, child, f"{context}.children[{idx}]", gen_ctx)

    gen_ctx.summary.custom_element_count += 1
    return elem


def _build_package(ar_packages: ET.Element, package: Dict[str, Any], package_index: int, gen_ctx: GenerationContext) -> None:
    package_ctx = f"autosar.packages[{package_index}]"
    package_map = _as_mapping(package, package_ctx)
    _warn_unknown_keys(
        package_map,
        {
            "short_name",
            "communication_cluster",
            "communication_clusters",
            "ethernet_communication_connector",
            "ethernet_communication_connectors",
            "someip",
            "dds",
            "zerocopy",
            "provided_someip_service_instances",
            "required_someip_service_instances",
            "service_interfaces",
            "service_interface",
            "execution_manifests",
            "execution_manifest",
            "custom_elements",
            "runtime",
        },
        package_ctx,
        gen_ctx,
    )

    package_element = ET.SubElement(ar_packages, "AR-PACKAGE")
    _add_text(package_element, "SHORT-NAME", _as_str(package_map.get("short_name"), f"{package_ctx}.short_name"))

    elements = ET.SubElement(package_element, "ELEMENTS")

    communication_clusters = []
    communication_clusters.extend(_listify(package_map.get("communication_clusters"), f"{package_ctx}.communication_clusters"))
    communication_clusters.extend(_listify(package_map.get("communication_cluster"), f"{package_ctx}.communication_cluster"))

    for idx, cluster in enumerate(communication_clusters):
        _build_communication_cluster(elements, cluster, f"{package_ctx}.communication_clusters[{idx}]", gen_ctx)

    connectors = []
    connectors.extend(_listify(package_map.get("ethernet_communication_connectors"), f"{package_ctx}.ethernet_communication_connectors"))
    connectors.extend(_listify(package_map.get("ethernet_communication_connector"), f"{package_ctx}.ethernet_communication_connector"))

    for idx, connector in enumerate(connectors):
        _build_connector(elements, connector, f"{package_ctx}.ethernet_communication_connectors[{idx}]", gen_ctx)

    someip = _as_mapping(package_map.get("someip", {}), f"{package_ctx}.someip", required=False)
    _warn_unknown_keys(
        someip,
        {
            "provided_service_instances",
            "provided_someip_service_instances",
            "required_service_instances",
            "required_someip_service_instances",
        },
        f"{package_ctx}.someip",
        gen_ctx,
    )

    provided_instances = []
    provided_instances.extend(_listify(someip.get("provided_service_instances"), f"{package_ctx}.someip.provided_service_instances"))
    provided_instances.extend(_listify(someip.get("provided_someip_service_instances"), f"{package_ctx}.someip.provided_someip_service_instances"))
    provided_instances.extend(_listify(package_map.get("provided_someip_service_instances"), f"{package_ctx}.provided_someip_service_instances"))

    required_instances = []
    required_instances.extend(_listify(someip.get("required_service_instances"), f"{package_ctx}.someip.required_service_instances"))
    required_instances.extend(_listify(someip.get("required_someip_service_instances"), f"{package_ctx}.someip.required_someip_service_instances"))
    required_instances.extend(_listify(package_map.get("required_someip_service_instances"), f"{package_ctx}.required_someip_service_instances"))

    for idx, instance in enumerate(provided_instances):
        _build_provided_someip_instance(elements, instance, f"{package_ctx}.provided_someip_service_instances[{idx}]", gen_ctx)

    for idx, instance in enumerate(required_instances):
        _build_required_someip_instance(elements, instance, f"{package_ctx}.required_someip_service_instances[{idx}]", gen_ctx)

    service_interfaces = []
    service_interfaces.extend(_listify(package_map.get("service_interfaces"), f"{package_ctx}.service_interfaces"))
    service_interfaces.extend(_listify(package_map.get("service_interface"), f"{package_ctx}.service_interface"))

    for idx, si in enumerate(service_interfaces):
        _build_service_interface(elements, si, f"{package_ctx}.service_interfaces[{idx}]", gen_ctx)

    execution_manifests = []
    execution_manifests.extend(_listify(package_map.get("execution_manifests"), f"{package_ctx}.execution_manifests"))
    execution_manifests.extend(_listify(package_map.get("execution_manifest"), f"{package_ctx}.execution_manifest"))

    for idx, em in enumerate(execution_manifests):
        _build_execution_manifest(elements, em, f"{package_ctx}.execution_manifests[{idx}]", gen_ctx)

    custom_elements = _listify(package_map.get("custom_elements"), f"{package_ctx}.custom_elements")

    _build_dds_binding(elements, package_map.get("dds"), f"{package_ctx}.dds", gen_ctx)

    has_non_standard_extension = (
        package_map.get("zerocopy") is not None
        or bool(custom_elements)
    )

    if has_non_standard_extension and not gen_ctx.allow_extensions:
        raise ConfigError(
            f"{package_ctx}: non-standard extension sections detected "
            "(zerocopy / custom_elements). "
            "For AUTOSAR-standard-only output, remove them. "
            "If you intentionally need project-specific extensions, "
            "run with --allow-extensions.")

    if gen_ctx.allow_extensions:
        _build_zerocopy_binding(elements, package_map.get("zerocopy"), f"{package_ctx}.zerocopy", gen_ctx)

    for idx, custom_element in enumerate(custom_elements):
        _build_custom_element(elements, custom_element, f"{package_ctx}.custom_elements[{idx}]", gen_ctx)

    gen_ctx.summary.package_count += 1


def build_arxml_tree(merged_config: Dict[str, Any], gen_ctx: Optional[GenerationContext] = None) -> ET.Element:
    if gen_ctx is None:
        gen_ctx = GenerationContext()

    autosar_map = _as_mapping(merged_config.get("autosar"), "autosar")
    _warn_unknown_keys(autosar_map, {"schema_namespace", "schema_location", "packages", "package"}, "autosar", gen_ctx)

    schema_namespace = _as_str(autosar_map.get("schema_namespace", DEFAULT_SCHEMA_NAMESPACE), "autosar.schema_namespace")
    schema_location = _as_str(autosar_map.get("schema_location", DEFAULT_SCHEMA_LOCATION), "autosar.schema_location")

    packages = []
    packages.extend(_listify(autosar_map.get("packages"), "autosar.packages"))
    packages.extend(_listify(autosar_map.get("package"), "autosar.package"))
    if not packages:
        raise ConfigError("autosar.packages is empty")

    ET.register_namespace("xsi", XSI_NAMESPACE)
    root = ET.Element(
        "AUTOSAR",
        {
            "xmlns": schema_namespace,
            "xmlns:xsi": XSI_NAMESPACE,
            "xsi:schemaLocation": schema_location,
        },
    )

    ar_packages = ET.SubElement(root, "AR-PACKAGES")
    for package_index, package in enumerate(packages):
        _build_package(ar_packages, package, package_index, gen_ctx)

    return root


def serialize_tree(root: ET.Element, indent: int = 4) -> str:
    if indent < 0:
        raise ConfigError("indent must be zero or positive")

    raw_xml = ET.tostring(root, encoding="utf-8")
    pretty_xml = minidom.parseString(raw_xml).toprettyxml(indent=" " * indent, encoding="UTF-8")
    lines = [line for line in pretty_xml.decode("utf-8").splitlines() if line.strip()]
    return "\n".join(lines) + "\n"


def generate_arxml_from_files(
    input_paths: List[Path],
    indent: int,
    strict: bool,
    allow_extensions: bool,
) -> Tuple[str, GenerationContext]:
    if not input_paths:
        raise ConfigError("At least one input YAML file is required")

    documents = [_load_yaml(path) for path in input_paths]
    merged = _merge_documents(documents)

    gen_ctx = GenerationContext(
        strict=strict,
        allow_extensions=allow_extensions)
    root = build_arxml_tree(merged, gen_ctx)
    xml_text = serialize_tree(root, indent=indent)
    return xml_text, gen_ctx


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate AUTOSAR ARXML from YAML communication definitions.")
    parser.add_argument(
        "-i",
        "--input",
        action="append",
        required=True,
        help="Input YAML file. Can be specified multiple times.",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Output ARXML file path. If omitted, ARXML is printed to stdout.",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Validate and parse input without writing ARXML output.",
    )
    parser.add_argument(
        "--indent",
        type=int,
        default=4,
        help="Indent width for pretty-printed XML (default: 4).",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail on unknown YAML keys.",
    )
    parser.add_argument(
        "--allow-extensions",
        action="store_true",
        help=(
            "Allow non-standard extension sections "
            "(zerocopy / custom_elements). "
            "Without this flag, generator enforces AUTOSAR-standard-only profile."
        ),
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Allow overwriting existing output file.",
    )
    parser.add_argument(
        "--print-summary",
        action="store_true",
        help="Print generation summary to stderr.",
    )
    return parser


def _print_summary(gen_ctx: GenerationContext, output_target: str) -> None:
    summary = gen_ctx.summary
    lines = [
        "[arxml-generator] generation summary",
        f"  output: {output_target}",
        f"  packages: {summary.package_count}",
        f"  communication clusters: {summary.communication_cluster_count}",
        f"  ethernet connectors: {summary.connector_count}",
        f"  provided SOME/IP instances: {summary.provided_someip_count}",
        f"  required SOME/IP instances: {summary.required_someip_count}",
        f"  DDS bindings: {summary.dds_binding_count}",
        f"  ZeroCopy bindings: {summary.zerocopy_binding_count}",
        f"  service interfaces: {summary.service_interface_count}",
        f"  execution manifests: {summary.execution_manifest_count}",
        f"  custom elements: {summary.custom_element_count}",
    ]

    if gen_ctx.warnings:
        lines.append(f"  warnings: {len(gen_ctx.warnings)}")
        for warning in gen_ctx.warnings:
            lines.append(f"    - {warning}")
    else:
        lines.append("  warnings: 0")

    sys.stderr.write("\n".join(lines) + "\n")


def main(argv: Optional[List[str]] = None) -> int:
    parser = _build_arg_parser()
    args = parser.parse_args(argv)

    input_paths = [Path(path) for path in args.input]

    try:
        xml_text, gen_ctx = generate_arxml_from_files(
            input_paths=input_paths,
            indent=args.indent,
            strict=args.strict,
            allow_extensions=args.allow_extensions,
        )
    except ConfigError as exc:
        sys.stderr.write(f"[arxml-generator] error: {exc}\n")
        return 2

    if args.validate_only:
        output_target = "<validate-only>"
        if args.print_summary:
            _print_summary(gen_ctx, output_target)
        return 0

    if args.output:
        output_path = Path(args.output)
        if output_path.exists() and not args.overwrite:
            sys.stderr.write(
                f"[arxml-generator] error: output file already exists: {output_path}. "
                "Use --overwrite to replace it.\n")
            return 2

        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(xml_text, encoding="utf-8")
        output_target = str(output_path)
    else:
        sys.stdout.write(xml_text)
        output_target = "<stdout>"

    if args.print_summary:
        _print_summary(gen_ctx, output_target)

    return 0


if __name__ == "__main__":
    sys.exit(main())
