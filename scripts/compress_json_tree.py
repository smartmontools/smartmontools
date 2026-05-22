#!/usr/bin/env python3
"""Compress smartctl JSON output by keeping only the first element of each list.

Usage:
    compress_json_tree.py <input.json> [output.json]
    compress_json_tree.py <input.json> -   # print to stdout
    compress_json_tree.py *.json           # compress all, write to *.tree.json

If no output is given, prints to stdout.
"""

import json
import sys
from pathlib import Path


def _alt_repr(v):
    if isinstance(v, dict):
        scalars = [(k, val) for k, val in v.items() if not isinstance(val, (dict, list))]
        if scalars:
            preview = ", ".join(f"{k}: {repr(val)}" for k, val in scalars[:2])
            return "{" + preview + ("}" if len(scalars) <= 2 else ", ...}")
        return "{...}"
    if isinstance(v, list):
        return "[...]"
    if v is None:
        return "null"
    return repr(v)


def _dedup_key(v):
    if isinstance(v, dict):
        scalars = [(k, val) for k, val in v.items() if not isinstance(val, (dict, list))]
        # skip the first scalar (usually a unique id/address), deduplicate on the rest
        rest = scalars[1:] if len(scalars) > 1 else scalars
        return tuple(rest)
    return v


def compress(obj):
    if isinstance(obj, list):
        if not obj:
            return []
        result = [compress(obj[0])]
        if len(obj) > 1:
            if all(not isinstance(v, (dict, list)) for v in obj[1:]):
                # scalar list: show sorted unique values
                unique = sorted(set(obj[1:]), key=lambda v: (v is None, v))
                parts = [_alt_repr(v) for v in unique]
            else:
                seen_reprs = []
                seen_keys = []
                counts = {}
                for v in obj[1:]:
                    key = _dedup_key(v)
                    if key not in counts:
                        seen_reprs.append(_alt_repr(v))
                        seen_keys.append(key)
                    counts[key] = counts.get(key, 0) + 1
                parts = [r if counts[k] == 1 else f"{r} x{counts[k]}"
                         for r, k in zip(seen_reprs, seen_keys)]
            result.append(f"// alternatives: {', '.join(parts)}")
        return result
    if isinstance(obj, dict):
        return {k: compress(v) for k, v in obj.items()}
    return obj


def process_file(input_path, output_path=None):
    with open(input_path) as f:
        data = json.load(f)
    compressed = compress(data)
    text = json.dumps(compressed, indent=2)
    if output_path is None or output_path == "-":
        print(text)
    else:
        with open(output_path, "w") as f:
            f.write(text + "\n")


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)

    if len(args) == 1:
        process_file(args[0])
    elif len(args) == 2:
        process_file(args[0], args[1])
    else:
        for path in args:
            p = Path(path)
            if p.suffix == ".json" and not p.stem.endswith(".tree"):
                out = p.with_name(p.stem + ".tree.json")
                process_file(p, out)
                print(f"{p} -> {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
