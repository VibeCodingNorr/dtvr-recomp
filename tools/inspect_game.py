#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only inventory of an original DTVR installation; JSON on stdout."""

import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct


def inspect_ne(path):
    data = path.read_bytes()

    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError(f"{path}: truncated executable at {offset:#x}")
        return struct.unpack_from(fmt, data, offset)

    if data[:2] != b"MZ":
        raise ValueError(f"{path}: missing MZ signature")
    ne = unpack("<I", 0x3C)[0]
    if data[ne:ne + 2] != b"NE":
        raise ValueError(f"{path}: missing NE signature")

    def word(offset):
        return unpack("<H", ne + offset)[0]

    modules = []
    for i in range(word(0x1E)):
        offset = ne + word(0x2A) + unpack("<H", ne + word(0x28) + i * 2)[0]
        size = unpack("<B", offset)[0]
        raw = unpack(f"<{size}s", offset + 1)[0]
        modules.append(raw.decode("ascii"))
    segments = []
    for i in range(word(0x1C)):
        sector, length, flags, minimum = unpack("<4H", ne + word(0x22) + i * 8)
        segments.append({"number": i + 1, "kind": "data" if flags & 1 else "code",
                         "file_offset": sector << word(0x32),
                         "size": length or 65536, "flags": flags,
                         "minimum_allocation": minimum or 65536})
    return {"sha256": hashlib.sha256(data).hexdigest(), "size": len(data),
            "format": "NE", "imports": modules, "segments": segments,
            "entry_point": {"segment": word(0x16), "offset": word(0x14)}}


def inspect_game(root):
    if (root / "program/rally.exe").is_file():
        root = root / "program"
    if not (root / "rally.exe").is_file():
        raise ValueError(f"{root}: rally.exe not found")
    files = sorted(p for p in root.rglob("*") if p.is_file())
    return {"program_directory": str(root.resolve()),
            "file_count": len(files), "total_bytes": sum(p.stat().st_size for p in files),
            "extensions": dict(sorted(collections.Counter(p.suffix.lower() or "(none)" for p in files).items())),
            "executable": inspect_ne(root / "rally.exe")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="CD root or program directory")
    args = parser.parse_args()
    try:
        result = inspect_game(args.data)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f"Error: {error}\n")
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
