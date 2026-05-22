#!/usr/bin/env python3
"""Compare SMARTMON MIB definitions with an snmpwalk-style output file.

Default usage from the repository root:

    scripts/compare-snmp-mib.py

The report answers two practical questions:
  * Which MIB objects are present in the SNMP output?
  * Which MIB tables have data in the SNMP output?
"""

from __future__ import annotations

import argparse
import glob
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path


OBJECT_START_RE = re.compile(r"^([A-Za-z][A-Za-z0-9-]*)\s+OBJECT-TYPE\b")
OBJECT_ID_RE = re.compile(r"^([A-Za-z][A-Za-z0-9-]*)\s+OBJECT IDENTIFIER\s+::=")
MODULE_ID_RE = re.compile(r"^([A-Za-z][A-Za-z0-9-]*)\s+MODULE-IDENTITY\b")
OUTPUT_OBJECT_RE = re.compile(r"^([^:\s]+)::([A-Za-z][A-Za-z0-9-]*)(?:\.|\s|$)")
OUTPUT_VALUE_RE = re.compile(r"^([^:\s]+)::([A-Za-z][A-Za-z0-9-]*)\.[^=]+ = [^:]+:\s*(-?\d+)")


@dataclass
class MibObject:
    module: str
    name: str
    kind: str
    file: Path
    line: int
    parent: str | None = None
    syntax: str | None = None
    entry_type: str | None = None
    max_access: str | None = None


@dataclass
class MibTable:
    module: str
    name: str
    file: Path
    line: int
    entry_name: str | None = None
    entry_type: str | None = None
    columns: set[str] = field(default_factory=set)
    metadata: set[str] = field(default_factory=set)


def mib_module_name(path: Path, lines: list[str]) -> str:
    for line in lines:
        stripped = line.strip()
        if stripped and " DEFINITIONS ::= BEGIN" in stripped:
            return stripped.split()[0]
    return path.stem


def parse_object_block(lines: list[str], start: int) -> tuple[list[str], int]:
    block: list[str] = []
    idx = start
    while idx < len(lines):
        line = lines[idx].rstrip("\n")
        block.append(line)
        if "::=" in line and idx > start:
            break
        idx += 1
    return block, idx


def parse_mib(path: Path) -> tuple[str, dict[str, MibObject], dict[str, MibTable]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    module = mib_module_name(path, lines)
    objects: dict[str, MibObject] = {}
    tables: dict[str, MibTable] = {}

    pending_entries: dict[str, tuple[str, str]] = {}

    idx = 0
    while idx < len(lines):
        stripped = lines[idx].strip()

        object_id = OBJECT_ID_RE.match(stripped)
        if object_id:
            name = object_id.group(1)
            parent = parse_parent(stripped)
            objects[name] = MibObject(module, name, "object-identifier", path, idx + 1, parent=parent)
            idx += 1
            continue

        module_id = MODULE_ID_RE.match(stripped)
        if module_id:
            name = module_id.group(1)
            block, end = parse_object_block(lines, idx)
            objects[name] = MibObject(module, name, "module-identity", path, idx + 1, parent=parse_parent("\n".join(block)))
            idx = end + 1
            continue

        object_start = OBJECT_START_RE.match(stripped)
        if not object_start:
            idx += 1
            continue

        name = object_start.group(1)
        block, end = parse_object_block(lines, idx)
        block_text = "\n".join(block)
        syntax = parse_syntax(block_text)
        parent = parse_parent(block_text)
        max_access = parse_max_access(block_text)
        kind = "object-type"
        entry_type: str | None = None

        sequence_match = re.search(r"\bSYNTAX\s+SEQUENCE OF\s+([A-Za-z][A-Za-z0-9-]*)", block_text)
        if sequence_match:
            kind = "table"
            entry_type = sequence_match.group(1)
            tables[name] = MibTable(module, name, path, idx + 1, entry_type=entry_type)
            pending_entries[entry_type] = (name, module)
        elif syntax in pending_entries and re.search(r"\bINDEX\s*\{", block_text):
            kind = "entry"
            table_name, _ = pending_entries[syntax]
            tables[table_name].entry_name = name

        objects[name] = MibObject(
            module,
            name,
            kind,
            path,
            idx + 1,
            parent=parent,
            syntax=syntax,
            entry_type=entry_type,
            max_access=max_access,
        )
        idx = end + 1

    for obj in objects.values():
        if obj.kind != "object-type" or not obj.parent:
            continue
        for table in tables.values():
            if obj.parent == table.entry_name:
                table.columns.add(obj.name)
            elif obj.name in {f"{table.name}RowCount", f"{table.name}LastChange"}:
                table.metadata.add(obj.name)

    return module, objects, tables


def parse_syntax(block_text: str) -> str | None:
    match = re.search(r"\bSYNTAX\s+([^\n]+)", block_text)
    if not match:
        return None
    return match.group(1).strip().split()[0]


def parse_max_access(block_text: str) -> str | None:
    match = re.search(r"\bMAX-ACCESS\s+([^\n]+)", block_text)
    if not match:
        return None
    return match.group(1).strip().split()[0]


def parse_parent(text: str) -> str | None:
    match = re.search(r"::=\s*\{\s*([A-Za-z][A-Za-z0-9-]*)\s+\d+\s*\}", text, re.MULTILINE)
    return match.group(1) if match else None


def parse_output(path: Path) -> tuple[dict[str, set[str]], dict[tuple[str, str], int]]:
    used: dict[str, set[str]] = {}
    values: dict[tuple[str, str], int] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        match = OUTPUT_OBJECT_RE.match(line)
        if not match:
            continue
        module, name = match.groups()
        used.setdefault(module, set()).add(name)
        value_match = OUTPUT_VALUE_RE.match(line)
        if value_match:
            value_module, value_name, value = value_match.groups()
            values[(value_module, value_name)] = int(value)
    return used, values


def readable_objects(objects: dict[str, MibObject]) -> set[str]:
    return {
        name
        for name, obj in objects.items()
        if obj.kind == "object-type" and obj.max_access and obj.max_access != "not-accessible"
    }


def module_sort_key(module: str) -> tuple[int, str]:
    preferred = {
        "SMARTMON-COMMON-MIB": 0,
        "SMARTMON-NVME-MIB": 1,
        "SMARTMON-SATA-MIB": 2,
        "SMARTMON-SAS-MIB": 3,
        "SMARTMON-SENSOR-MIB": 4,
        "SMARTMON-TC-MIB": 5,
    }
    return preferred.get(module, 99), module


def print_report(
    mib_objects: dict[str, dict[str, MibObject]],
    mib_tables: dict[str, dict[str, MibTable]],
    used: dict[str, set[str]],
    values: dict[tuple[str, str], int],
    output_path: Path,
    show_present: bool,
) -> tuple[int, int]:
    total_defined = sum(len(items) for items in mib_objects.values())
    total_readable = sum(len(readable_objects(items)) for items in mib_objects.values())
    total_used = sum(len(items) for items in used.values())
    known_used = 0
    extra_used = 0

    for module, names in used.items():
        known = mib_objects.get(module, {})
        known_used += len(names & known.keys())
        extra_used += len(names - known.keys())

    print("# SNMP/MIB comparison summary")
    print()
    print(f"Output file: `{output_path}`")
    print()
    print("| Metric | Count |")
    print("| --- | ---: |")
    print(f"| MIB modules parsed | {len(mib_objects)} |")
    print(f"| MIB objects defined | {total_defined} |")
    print(f"| Readable MIB objects defined | {total_readable} |")
    print(f"| Unique output objects | {total_used} |")
    print(f"| Output objects found in MIBs | {known_used} |")
    print(f"| Output objects not found in MIBs | {extra_used} |")
    print()

    print("## Modules")
    print()
    print("| Module | Readable objects | Used readable objects | Missing readable objects | Extra in output |")
    print("| --- | ---: | ---: | ---: | ---: |")
    all_modules = sorted(set(mib_objects) | set(used), key=module_sort_key)
    for module in all_modules:
        all_defined = set(mib_objects.get(module, {}))
        defined = readable_objects(mib_objects.get(module, {}))
        used_names = used.get(module, set())
        print(
            f"| {module} | {len(defined)} | {len(used_names & defined)} | "
            f"{len(defined - used_names)} | {len(used_names - all_defined)} |"
        )
    print()

    print("## Tables")
    print()
    print("| Module | Table | Columns used | Columns defined | Metadata used | Status |")
    print("| --- | --- | ---: | ---: | --- | --- |")
    for module in all_modules:
        for table in sorted(mib_tables.get(module, {}).values(), key=lambda item: item.name):
            used_names = used.get(module, set())
            used_columns = table.columns & used_names
            used_metadata = table.metadata & used_names
            row_count = values.get((module, f"{table.name}RowCount"))
            if used_columns:
                status = "used"
            elif row_count == 0:
                status = "empty"
            elif used_metadata:
                status = "metadata only"
            else:
                status = "not used"
            metadata = ", ".join(sorted(used_metadata)) or "-"
            print(
                f"| {module} | {table.name} | {len(used_columns)} | {len(table.columns)} | "
                f"{metadata} | {status} |"
            )
    print()

    if show_present:
        print("## Objects Used By Output")
        print()
        for module in all_modules:
            defined = set(mib_objects.get(module, {}))
            used_names = used.get(module, set())
            if not used_names:
                continue
            print(f"### {module}")
            print()
            print("Used and defined:")
            print(format_name_list(sorted(used_names & defined)))
            extras = sorted(used_names - defined)
            if extras:
                print("Not found in parsed MIBs:")
                print(format_name_list(extras))
            print()
    elif extra_used:
        print("## Output Objects Not Found In Parsed MIBs")
        print()
        for module in all_modules:
            defined = set(mib_objects.get(module, {}))
            extras = sorted(used.get(module, set()) - defined)
            if not extras:
                continue
            print(f"### {module}")
            print(format_name_list(extras))
            print()

    print("## Readable Objects Not Seen In Output")
    print()
    for module in all_modules:
        missing = sorted(readable_objects(mib_objects.get(module, {})) - used.get(module, set()))
        if not missing:
            continue
        print(f"### {module}")
        print(format_name_list(missing))
        print()

    return known_used, extra_used


def format_name_list(names: list[str]) -> str:
    if not names:
        return "-\n"
    return "\n".join(f"- `{name}`" for name in names) + "\n"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mib-glob", default="doc/*mib", help="Glob for MIB files; default: doc/*mib")
    parser.add_argument(
        "--output",
        default=".tmp/output/pve2.snmp",
        type=Path,
        help="snmpwalk-style output file; default: .tmp/output/pve2.snmp",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero if the output contains objects not defined in the parsed MIBs.",
    )
    parser.add_argument(
        "--show-present",
        action="store_true",
        help="Include the long list of output objects that are present in the parsed MIBs.",
    )
    args = parser.parse_args(argv)

    mib_paths = [Path(path) for path in sorted(glob.glob(args.mib_glob))]
    if not mib_paths:
        print(f"No MIB files matched: {args.mib_glob}", file=sys.stderr)
        return 2
    if not args.output.exists():
        print(f"Output file not found: {args.output}", file=sys.stderr)
        return 2

    mib_objects: dict[str, dict[str, MibObject]] = {}
    mib_tables: dict[str, dict[str, MibTable]] = {}
    for path in mib_paths:
        module, objects, tables = parse_mib(path)
        mib_objects[module] = objects
        mib_tables[module] = tables

    used, values = parse_output(args.output)
    _, extra_used = print_report(mib_objects, mib_tables, used, values, args.output, args.show_present)

    if args.strict and extra_used:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
