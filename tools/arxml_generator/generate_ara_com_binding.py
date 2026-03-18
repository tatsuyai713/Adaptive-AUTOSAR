#!/usr/bin/env python3
"""Generate ara::com binding constants header from ARXML."""

from __future__ import annotations

import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Optional


class GenerationError(RuntimeError):
    """Raised when ARXML does not contain the expected elements."""


@dataclass(frozen=True)
class SomeIpBinding:
    provided_service_short_name: str
    provided_event_group_short_name: str
    service_interface_id: int
    service_instance_id: int
    major_version: int
    minor_version: int
    event_group_id: int
    event_id: int
    # Sequence of (method_short_name: str, method_id: int) tuples
    method_bindings: tuple = ()
    # Sequence of (event_group_short_name: str, event_id: int, group_id: int) for additional events
    extra_events: tuple = ()


def _find_child_text(element: ET.Element, tag_name: str) -> str:
    child = element.find(f"./{{*}}{tag_name}")
    if child is None or child.text is None:
        raise GenerationError(f"Missing required tag: {tag_name}")
    text = child.text.strip()
    if text == "":
        raise GenerationError(f"Tag has empty value: {tag_name}")
    return text


def _find_optional_child_text(element: ET.Element, tag_name: str) -> Optional[str]:
    child = element.find(f"./{{*}}{tag_name}")
    if child is None or child.text is None:
        return None
    text = child.text.strip()
    return text if text else None


def _parse_int(text: str, field_name: str) -> int:
    try:
        return int(text, 0)
    except ValueError as exc:
        raise GenerationError(f"Invalid integer in {field_name}: {text}") from exc


def _to_header_guard(output_path: pathlib.Path) -> str:
    guard = str(output_path).upper()
    for token in ("/", "\\", ".", "-", " ", ":"):
        guard = guard.replace(token, "_")
    return f"GEN_{guard}_INCLUDED"


def _to_pascal_case(name: str) -> str:
    """Convert snake_case or kebab-case identifier to PascalCase.

    Names that contain no separators (already PascalCase/camelCase) are returned
    with just the first character uppercased to preserve the original casing.
    """
    if "_" not in name and "-" not in name:
        return name[0].upper() + name[1:] if name else name
    return "".join(word.capitalize() for word in name.replace("-", "_").split("_"))


def _open_namespace(namespace_value: str) -> str:
    parts = [part.strip() for part in namespace_value.split("::") if part.strip()]
    if not parts:
        raise GenerationError("Namespace must not be empty.")
    return "\n".join([f"namespace {part} {{" for part in parts])


def _close_namespace(namespace_value: str) -> str:
    parts = [part.strip() for part in namespace_value.split("::") if part.strip()]
    return "\n".join(["}" for _ in parts])


def extract_someip_binding(
    root: ET.Element,
    provided_service_short_name: Optional[str],
    provided_event_group_short_name: Optional[str],
) -> SomeIpBinding:
    provided_instances = root.findall(".//{*}PROVIDED-SOMEIP-SERVICE-INSTANCE")
    if not provided_instances:
        raise GenerationError("No PROVIDED-SOMEIP-SERVICE-INSTANCE found in ARXML.")

    selected_instance: Optional[ET.Element] = None
    for instance in provided_instances:
        short_name = _find_optional_child_text(instance, "SHORT-NAME")
        if provided_service_short_name is None:
            selected_instance = instance
            break
        if short_name == provided_service_short_name:
            selected_instance = instance
            break

    if selected_instance is None:
        raise GenerationError(
            "Could not find PROVIDED-SOMEIP-SERVICE-INSTANCE with SHORT-NAME="
            f"{provided_service_short_name!r}."
        )

    instance_short_name = _find_child_text(selected_instance, "SHORT-NAME")

    deployment = selected_instance.find("./{*}SERVICE-INTERFACE-DEPLOYMENT")
    if deployment is None:
        raise GenerationError(
            "Missing SERVICE-INTERFACE-DEPLOYMENT in PROVIDED-SOMEIP-SERVICE-INSTANCE."
        )

    version = deployment.find("./{*}SERVICE-INTERFACE-VERSION")
    if version is None:
        raise GenerationError(
            "Missing SERVICE-INTERFACE-VERSION in SERVICE-INTERFACE-DEPLOYMENT."
        )

    service_interface_id = _parse_int(
        _find_child_text(deployment, "SERVICE-INTERFACE-ID"), "SERVICE-INTERFACE-ID"
    )
    major_version = _parse_int(_find_child_text(version, "MAJOR-VERSION"), "MAJOR-VERSION")
    minor_version = _parse_int(_find_child_text(version, "MINOR-VERSION"), "MINOR-VERSION")
    service_instance_id = _parse_int(
        _find_child_text(selected_instance, "SERVICE-INSTANCE-ID"), "SERVICE-INSTANCE-ID"
    )

    provided_event_groups = selected_instance.findall(
        "./{*}PROVIDED-EVENT-GROUPS/{*}SOMEIP-PROVIDED-EVENT-GROUP"
    )
    if not provided_event_groups:
        raise GenerationError(
            "No SOMEIP-PROVIDED-EVENT-GROUP found under PROVIDED-EVENT-GROUPS."
        )

    selected_event_group: Optional[ET.Element] = None
    for event_group in provided_event_groups:
        short_name = _find_optional_child_text(event_group, "SHORT-NAME")
        if provided_event_group_short_name is None:
            selected_event_group = event_group
            break
        if short_name == provided_event_group_short_name:
            selected_event_group = event_group
            break

    if selected_event_group is None:
        raise GenerationError(
            "Could not find SOMEIP-PROVIDED-EVENT-GROUP with SHORT-NAME="
            f"{provided_event_group_short_name!r}."
        )

    event_group_short_name = _find_child_text(selected_event_group, "SHORT-NAME")
    event_group_id = _parse_int(
        _find_child_text(selected_event_group, "EVENT-GROUP-ID"), "EVENT-GROUP-ID"
    )
    event_id = _parse_int(_find_child_text(selected_event_group, "EVENT-ID"), "EVENT-ID")

    # Collect additional event groups (all groups except the primary one).
    extra_events_list = []
    for eg in provided_event_groups:
        eg_name = _find_optional_child_text(eg, "SHORT-NAME") or ""
        if eg_name == event_group_short_name:
            continue
        eg_event_id_text = _find_optional_child_text(eg, "EVENT-ID")
        eg_group_id_text = _find_optional_child_text(eg, "EVENT-GROUP-ID")
        if eg_event_id_text and eg_group_id_text:
            extra_events_list.append((
                eg_name,
                _parse_int(eg_event_id_text, "EVENT-ID"),
                _parse_int(eg_group_id_text, "EVENT-GROUP-ID"),
            ))

    # Collect SOME/IP method deployments (METHOD-DEPLOYMENTS/SOMEIP-METHOD-DEPLOYMENT).
    method_elements = selected_instance.findall(
        "./{*}SERVICE-INTERFACE-DEPLOYMENT/{*}METHOD-DEPLOYMENTS/{*}SOMEIP-METHOD-DEPLOYMENT"
    )
    method_bindings_list = []
    for me in method_elements:
        me_name = _find_optional_child_text(me, "SHORT-NAME") or ""
        me_id_text = _find_optional_child_text(me, "METHOD-ID")
        if me_name and me_id_text:
            method_bindings_list.append((me_name, _parse_int(me_id_text, "METHOD-ID")))

    return SomeIpBinding(
        provided_service_short_name=instance_short_name,
        provided_event_group_short_name=event_group_short_name,
        service_interface_id=service_interface_id,
        service_instance_id=service_instance_id,
        major_version=major_version,
        minor_version=minor_version,
        event_group_id=event_group_id,
        event_id=event_id,
        method_bindings=tuple(method_bindings_list),
        extra_events=tuple(extra_events_list),
    )


def generate_header_text(
    binding: SomeIpBinding,
    output_path: pathlib.Path,
    namespace_value: str,
) -> str:
    guard = _to_header_guard(output_path)
    open_ns = _open_namespace(namespace_value)
    close_ns = _close_namespace(namespace_value)

    # Build dynamic method ID constants.
    method_constants = ""
    for method_name, method_id in binding.method_bindings:
        pascal = _to_pascal_case(method_name)
        method_constants += f"constexpr std::uint16_t kMethod{pascal}Id{{0x{method_id:04X}U}};\n"

    # Build extra event group constants (for services with multiple event groups).
    extra_event_constants = ""
    for event_name, ev_id, grp_id in binding.extra_events:
        pascal = _to_pascal_case(event_name)
        extra_event_constants += f"constexpr std::uint16_t k{pascal}EventId{{0x{ev_id:04X}U}};\n"
        extra_event_constants += f"constexpr std::uint16_t k{pascal}EventGroupId{{0x{grp_id:04X}U}};\n"

    optional_section = ""
    if method_constants or extra_event_constants:
        optional_section = method_constants + extra_event_constants

    return f"""#ifndef {guard}
#define {guard}

#include <cstdint>

// Auto-generated from ARXML. Do not edit manually.
{open_ns}
constexpr std::uint16_t kServiceId{{0x{binding.service_interface_id:04X}U}};
constexpr std::uint16_t kInstanceId{{0x{binding.service_instance_id:04X}U}};
constexpr std::uint16_t kStatusEventId{{0x{binding.event_id:04X}U}};
constexpr std::uint16_t kStatusEventGroupId{{0x{binding.event_group_id:04X}U}};
constexpr std::uint8_t kMajorVersion{{0x{binding.major_version:02X}U}};
constexpr std::uint8_t kMinorVersion{{0x{binding.minor_version:02X}U}};
constexpr const char *kProvidedServiceShortName{{"{binding.provided_service_short_name}"}};
constexpr const char *kProvidedEventGroupShortName{{"{binding.provided_event_group_short_name}"}};
{optional_section}{close_ns}

#endif
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate ara::com SOME/IP binding constants header from ARXML."
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Input ARXML file path.",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output header file path.",
    )
    parser.add_argument(
        "--namespace",
        default="sample::vehicle_status::generated",
        help="C++ namespace for generated constants.",
    )
    parser.add_argument(
        "--provided-service-short-name",
        default=None,
        help="SHORT-NAME of PROVIDED-SOMEIP-SERVICE-INSTANCE to use.",
    )
    parser.add_argument(
        "--provided-event-group-short-name",
        default=None,
        help="SHORT-NAME of SOMEIP-PROVIDED-EVENT-GROUP to use.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_path = pathlib.Path(args.input).resolve()
    output_path = pathlib.Path(args.output).resolve()

    if not input_path.exists():
        sys.stderr.write(f"[ara-com-codegen] error: input not found: {input_path}\n")
        return 1

    try:
        root = ET.parse(str(input_path)).getroot()
        binding = extract_someip_binding(
            root,
            args.provided_service_short_name,
            args.provided_event_group_short_name,
        )
        header_text = generate_header_text(
            binding=binding,
            output_path=output_path,
            namespace_value=args.namespace,
        )
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(header_text, encoding="utf-8")
    except (ET.ParseError, GenerationError) as exc:
        sys.stderr.write(f"[ara-com-codegen] error: {exc}\n")
        return 1

    method_count = len(binding.method_bindings)
    extra_event_count = len(binding.extra_events)
    print(
        "[ara-com-codegen] generated"
        f" service=0x{binding.service_interface_id:04X}"
        f" instance=0x{binding.service_instance_id:04X}"
        f" event=0x{binding.event_id:04X}"
        f" event_group=0x{binding.event_group_id:04X}"
        f" methods={method_count}"
        f" extra_events={extra_event_count}"
        f" -> {output_path}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
