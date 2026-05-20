#!/usr/bin/env python3
"""Convert an SNMP walk dump to YAML while preserving values verbatim.

By default this reads .tmp/output/pve2.snmp and writes YAML to stdout:

    scripts/snmp_to_yaml.py
    scripts/snmp_to_yaml.py .tmp/output/pve2.snmp -o .tmp/output/pve2.yaml

The text after " = " is kept exactly as it appears in the input line,
including the SNMP type prefix such as "Gauge32:" or "STRING:".
Rows are reordered by device index so all tables for one device are grouped.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from pathlib import Path


DEFAULT_INPUT = Path(".tmp/output/pve2.snmp")


@dataclass
class Table:
    mib: str
    name: str
    values: list[tuple[str, str]] = field(default_factory=list)


def yaml_quote(text: str) -> str:
    escaped = (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\t", "\\t")
        .replace("\r", "\\r")
    )
    return f'"{escaped}"'


def table_name_from_object(object_name: str, current_table: str | None) -> str:
    if object_name.endswith("TableRowCount"):
        return object_name[: -len("RowCount")]
    return current_table or "values"


def parse_snmp(path: Path) -> list[Table]:
    tables: list[Table] = []
    current: Table | None = None

    with path.open("r", encoding="utf-8", errors="replace") as snmp_file:
        for line_no, line in enumerate(snmp_file, 1):
            line = line.rstrip("\n")
            if not line:
                continue

            try:
                left, value = line.split(" = ", 1)
                mib, key = left.split("::", 1)
            except ValueError as err:
                raise ValueError(f"Cannot parse line {line_no}: {line}") from err

            object_name = key.split(".", 1)[0]
            table_name = table_name_from_object(object_name, current.name if current else None)

            if current is None or current.mib != mib or current.name != table_name:
                current = Table(mib=mib, name=table_name)
                tables.append(current)

            current.values.append((key, value))

    return tables


def device_from_key(key: str) -> int | None:
    try:
        index_text = key.split(".", 1)[1]
    except IndexError:
        return None

    indexes = index_text.split(".")
    if indexes == ["0"]:
        return None

    try:
        return int(indexes[0])
    except ValueError:
        return None


def object_name_from_key(key: str) -> str:
    return key.split(".", 1)[0]


def common_object_prefix(values: list[tuple[str, str]]) -> str:
    object_names = [object_name_from_key(key) for key, _value in values]
    if not object_names:
        return ""

    prefix = object_names[0]
    for object_name in object_names[1:]:
        while not object_name.startswith(prefix):
            prefix = prefix[:-1]
            if not prefix:
                return ""

    # Keep prefixes readable by stripping only at CamelCase word boundaries.
    while prefix and not any(name == prefix or name[len(prefix) : len(prefix) + 1].isupper() for name in object_names):
        prefix = prefix[:-1]

    return prefix


def strip_key_prefix(key: str, prefix: str) -> str:
    if not prefix or not key.startswith(prefix):
        return key

    stripped = key[len(prefix) :]
    return stripped or key


def split_key(key: str, prefix: str) -> tuple[str, list[str]]:
    key = strip_key_prefix(key, prefix)
    name, sep, index_text = key.partition(".")
    indexes = index_text.split(".") if sep else []
    return name, indexes


def row_groups(values: list[tuple[str, str]], prefix: str) -> dict[str, list[tuple[str, str]]] | None:
    rows: dict[str, list[tuple[str, str]]] = {}

    for key, value in values:
        name, indexes = split_key(key, prefix)
        if len(indexes) < 2:
            return None
        row = ".".join(indexes[1:])
        rows.setdefault(row, []).append((name, value))

    return rows if len(rows) > 1 else None


def row_sort_key(row: str) -> tuple[int, tuple[int, ...] | str]:
    try:
        return (0, tuple(int(part) for part in row.split(".")))
    except ValueError:
        return (1, row)


def render_table(table: Table, values: list[tuple[str, str]], indent: str, group_rows: bool) -> list[str]:
    value_indent = indent + "  "
    key_prefix = common_object_prefix(values)
    lines = [
        indent + "- mib: " + yaml_quote(table.mib),
        indent + "  table: " + yaml_quote(table.name),
    ]

    rows = row_groups(values, key_prefix) if group_rows else None
    if rows:
        row_indent = indent + "    "
        lines.append(indent + "  rows:")
        for row_idx, row in enumerate(sorted(rows, key=row_sort_key)):
            if row_idx:
                lines.append("")
            lines.append(row_indent + "- row: " + yaml_quote(row))
            lines.append(row_indent + "  values:")
            for key, value in rows[row]:
                lines.append(f"{row_indent}    {yaml_quote(key)}: {yaml_quote(value)}")
        return lines

    lines.append(indent + "  values:")
    for key, value in values:
        lines.append(f"{value_indent}  {yaml_quote(strip_key_prefix(key, key_prefix))}: {yaml_quote(value)}")
    return lines


def render_yaml(tables: list[Table]) -> str:
    metadata: list[tuple[Table, list[tuple[str, str]]]] = []
    devices: dict[int, list[tuple[Table, list[tuple[str, str]]]]] = {}

    for table in tables:
        table_metadata: list[tuple[str, str]] = []
        table_devices: dict[int, list[tuple[str, str]]] = {}

        for key, value in table.values:
            device = device_from_key(key)
            if device is None:
                table_metadata.append((key, value))
            else:
                table_devices.setdefault(device, []).append((key, value))

        if table_metadata:
            metadata.append((table, table_metadata))
        for device, values in table_devices.items():
            devices.setdefault(device, []).append((table, values))

    lines: list[str] = ["metadata:"]
    if metadata:
        for idx, (table, values) in enumerate(metadata):
            if idx:
                lines.append("")
            lines.extend(render_table(table, values, "  ", group_rows=False))
    else:
        lines.append("  []")

    lines.extend(["", "devices:"])
    if devices:
        for device_idx, device in enumerate(sorted(devices)):
            if device_idx:
                lines.append("")
            lines.append(f"  - device: {device}")
            lines.append("    tables:")
            for table_idx, (table, values) in enumerate(devices[device]):
                if table_idx:
                    lines.append("")
                lines.extend(render_table(table, values, "      ", group_rows=True))
    else:
        lines.append("  []")

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("-o", "--output", type=Path, help="write YAML to this file instead of stdout")
    args = parser.parse_args()

    yaml_text = render_yaml(parse_snmp(args.input))

    if args.output:
        args.output.write_text(yaml_text, encoding="utf-8")
    else:
        print(yaml_text, end="")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
