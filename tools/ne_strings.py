#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Dump RT_STRING resources of a 16-bit NE file as "id<TAB>text" lines.

Usage: ne_strings.py FILE [--encoding cp1252]

The output contains original game text: write it to an ignored, private
location (e.g. local/), never into the repository.
"""

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ne import NE, NEError  # noqa: E402

RT_STRING = 0x8006


def strings(data):
    ne = NE(data, segments=False)
    table = ne.ne + ne.word(0x24)
    if table >= len(data) or ne.word(0x24) == ne.word(0x26):
        return {}
    shift = struct.unpack_from("<H", data, table)[0]
    at = table + 2
    out = {}
    while at + 8 <= len(data):
        rtype, count = struct.unpack_from("<HH", data, at)
        if rtype == 0:
            break
        at += 8
        for _ in range(count):
            if at + 12 > len(data):
                raise NEError("resource table truncated")
            offset, length, _flags, rid = struct.unpack_from("<HHHH", data, at)
            at += 12
            if rtype != RT_STRING or not rid & 0x8000:
                continue
            start, size = offset << shift, length << shift
            if start + size > len(data):
                raise NEError("resource outside file")
            block = data[start:start + size]
            base = ((rid & 0x7FFF) - 1) * 16
            pos = 0
            for i in range(16):
                if pos >= len(block):
                    break
                n = block[pos]
                text = block[pos + 1:pos + 1 + n]
                pos += 1 + n
                if n:
                    out[base + i] = text
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", type=Path)
    ap.add_argument("--encoding", default="cp1252")
    args = ap.parse_args()
    for sid, text in sorted(strings(args.file.read_bytes()).items()):
        line = text.decode(args.encoding, "replace").replace("\r", "\\r").replace("\n", "\\n")
        print(f"{sid}\t{line}")


if __name__ == "__main__":
    main()
